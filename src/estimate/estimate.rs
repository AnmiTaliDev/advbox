use std::env;
use std::process::{Command, Stdio};
use std::time::{Duration, Instant};
use std::collections::VecDeque;
use std::io::{self, Write};

const HELP: &str = r#"
Estimate - Command execution time estimation tool

Usage: 
    estimate [OPTIONS] <command> [args...]

Options:
    -n, --iterations <N>    Number of iterations for averaging (default: 3)
    -w, --warmup <N>        Number of warmup runs (default: 1)
    -q, --quiet            Quiet mode - only show final results
    -s, --simple           Simple output format
    -h, --help             Show this help message

Example:
    estimate -n 5 ls -la
    estimate -w 2 -n 3 find . -type f
    estimate -s "sleep 1"

Note: Use quotes for commands with arguments
"#;

#[derive(Debug)]
struct Config {
    iterations: usize,
    warmup: usize,
    quiet: bool,
    simple: bool,
    command: String,
    args: Vec<String>,
}

#[derive(Debug)]
struct ExecutionStats {
    times: VecDeque<Duration>,
    min: Duration,
    max: Duration,
    avg: Duration,
    total_time: Duration,
    success_count: usize,
    fail_count: usize,
}

impl ExecutionStats {
    fn new() -> Self {
        ExecutionStats {
            times: VecDeque::new(),
            min: Duration::from_secs(0),
            max: Duration::from_secs(0),
            avg: Duration::from_secs(0),
            total_time: Duration::from_secs(0),
            success_count: 0,
            fail_count: 0,
        }
    }

    fn add_execution(&mut self, duration: Duration, success: bool) {
        self.times.push_back(duration);
        self.total_time += duration;

        if success {
            self.success_count += 1;
        } else {
            self.fail_count += 1;
        }

        // Update statistics
        if self.times.len() == 1 || duration < self.min {
            self.min = duration;
        }
        if self.times.len() == 1 || duration > self.max {
            self.max = duration;
        }

        // Recalculate the average
        self.avg = self.total_time / self.times.len() as u32;
    }
}

fn parse_args() -> Result<Config, String> {
    let args: Vec<String> = env::args().collect();
    
    if args.len() < 2 {
        return Err("No command specified".to_string());
    }

    let mut config = Config {
        iterations: 3,
        warmup: 1,
        quiet: false,
        simple: false,
        command: String::new(),
        args: Vec::new(),
    };

    let mut i = 1;
    while i < args.len() {
        match args[i].as_str() {
            "-h" | "--help" => {
                println!("{}", HELP);
                std::process::exit(0);
            }
            "-n" | "--iterations" => {
                i += 1;
                if i >= args.len() {
                    return Err("Missing value for iterations".to_string());
                }
                config.iterations = args[i].parse()
                    .map_err(|_| "Invalid iterations value")?;
                if config.iterations < 1 {
                    return Err("Iterations must be at least 1".to_string());
                }
            }
            "-w" | "--warmup" => {
                i += 1;
                if i >= args.len() {
                    return Err("Missing value for warmup".to_string());
                }
                config.warmup = args[i].parse()
                    .map_err(|_| "Invalid warmup value")?;
            }
            "-q" | "--quiet" => {
                config.quiet = true;
            }
            "-s" | "--simple" => {
                config.simple = true;
            }
            _ => {
                config.command = args[i].clone();
                config.args = args[i + 1..].to_vec();
                break;
            }
        }
        i += 1;
    }

    if config.command.is_empty() {
        return Err("No command specified".to_string());
    }

    Ok(config)
}

fn format_duration(duration: Duration) -> String {
    if duration.as_secs() > 0 {
        format!("{:.3}s", duration.as_secs_f64())
    } else {
        format!("{}ms", duration.as_millis())
    }
}

fn run_command(command: &str, args: &[String]) -> io::Result<(Duration, bool)> {
    let start = Instant::now();
    
    let status = Command::new(command)
        .args(args)
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .status()?;
    
    let duration = start.elapsed();
    Ok((duration, status.success()))
}

fn print_progress(current: usize, total: usize) {
    print!("\rProgress: [{:3}%] {}/{} ", 
           (current * 100) / total, 
           current, 
           total);
    io::stdout().flush().unwrap();
}

fn print_results(stats: &ExecutionStats, config: &Config) {
    if config.simple {
        println!("min={} max={} avg={} total={} success={} fail={}",
            format_duration(stats.min),
            format_duration(stats.max),
            format_duration(stats.avg),
            format_duration(stats.total_time),
            stats.success_count,
            stats.fail_count
        );
    } else {
        println!("\n=== Execution Summary ===");
        println!("Command: {} {}", config.command, config.args.join(" "));
        println!("Iterations: {}", stats.times.len());
        println!("Successful: {}", stats.success_count);
        println!("Failed: {}", stats.fail_count);
        println!("\nTimings:");
        println!("  Minimum: {}", format_duration(stats.min));
        println!("  Maximum: {}", format_duration(stats.max));
        println!("  Average: {}", format_duration(stats.avg));
        println!("  Total:   {}", format_duration(stats.total_time));
    }
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let config = match parse_args() {
        Ok(config) => config,
        Err(e) => {
            eprintln!("Error: {}", e);
            eprintln!("Try 'estimate --help' for more information.");
            std::process::exit(1);
        }
    };

    let total_runs = config.warmup + config.iterations;
    let mut stats = ExecutionStats::new();

    if !config.quiet {
        println!("Running '{}' {} times (including {} warmup runs)...",
                config.command,
                total_runs,
                config.warmup);
    }

    for i in 0..total_runs {
        if !config.quiet {
            print_progress(i + 1, total_runs);
        }

        match run_command(&config.command, &config.args) {
            Ok((duration, success)) => {
                if i >= config.warmup {
                    stats.add_execution(duration, success);
                }
            }
            Err(e) => {
                eprintln!("\nError executing command: {}", e);
                std::process::exit(1);
            }
        }
    }

    if !config.quiet {
        println!();
    }

    print_results(&stats, &config);

    Ok(())
}