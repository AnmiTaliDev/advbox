use std::env;
use std::process::{Command, exit};
use std::collections::HashMap;

const HELP: &str = r#"
KillPort - Kill processes using specified ports

Usage:
    killport [OPTIONS] <port1> [port2 ...]

Options:
    -f, --force     Force kill (SIGKILL instead of SIGTERM)
    -l, --list      Only list processes without killing
    -v, --verbose   Show detailed information
    -q, --quiet     Suppress all output except errors
    -h, --help      Show this help message

Examples:
    killport 8080
    killport -f 3000 8080
    killport -l 80 443
    
Note: Requires root privileges for ports below 1024
"#;

#[derive(Debug)]
struct Config {
    ports: Vec<u16>,
    force: bool,
    list_only: bool,
    verbose: bool,
    quiet: bool,
}

#[derive(Debug)]
struct ProcessInfo {
    pid: u32,
    name: String,
    user: String,
    protocol: String,
    state: String,
}

fn get_processes_by_port(port: u16) -> Vec<ProcessInfo> {
    let mut processes = Vec::new();
    
    // Получаем TCP соединения
    if let Ok(output) = Command::new("ss")
        .args(&["-tupln"])
        .output() {
        
        let output = String::from_utf8_lossy(&output.stdout);
        
        for line in output.lines().skip(1) { // Пропускаем заголовок
            let fields: Vec<&str> = line.split_whitespace().collect();
            if fields.len() >= 6 {
                // Проверяем, содержит ли строка наш порт
                if fields[3].contains(&format!(":{}", port)) {
                    // Извлекаем PID из последнего поля
                    if let Some(pid_str) = fields.last()
                        .and_then(|s| s.split(',').find(|s| s.starts_with("pid=")))
                        .and_then(|s| s.split('=').nth(1)) {
                        
                        if let Ok(pid) = pid_str.parse::<u32>() {
                            // Получаем информацию о процессе
                            if let Ok(proc_output) = Command::new("ps")
                                .args(&["-p", &pid.to_string(), "-o", "comm=,user="])
                                .output() {
                                
                                let proc_info = String::from_utf8_lossy(&proc_output.stdout);
                                let proc_fields: Vec<&str> = proc_info.split_whitespace().collect();
                                
                                if proc_fields.len() >= 2 {
                                    processes.push(ProcessInfo {
                                        pid,
                                        name: proc_fields[0].to_string(),
                                        user: proc_fields[1].to_string(),
                                        protocol: fields[0].to_string(),
                                        state: fields[1].to_string(),
                                    });
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    
    processes
}

fn kill_process(pid: u32, force: bool) -> bool {
    let signal = if force { "SIGKILL" } else { "SIGTERM" };
    Command::new("kill")
        .args(&[if force { "-9" } else { "-15" }, &pid.to_string()])
        .status()
        .map(|status| status.success())
        .unwrap_or(false)
}

fn print_process_info(proc: &ProcessInfo, port: u16, verbose: bool) {
    if verbose {
        println!("Port {} ({}):", port, proc.protocol);
        println!("  PID:      {}", proc.pid);
        println!("  Name:     {}", proc.name);
        println!("  User:     {}", proc.user);
        println!("  State:    {}", proc.state);
        println!();
    } else {
        println!("Port {}: {} (PID: {}, User: {})",
                port, proc.name, proc.pid, proc.user);
    }
}

fn main() {
    let args: Vec<String> = env::args().collect();
    let mut config = Config {
        ports: Vec::new(),
        force: false,
        list_only: false,
        verbose: false,
        quiet: false,
    };
    
    let mut i = 1;
    while i < args.len() {
        match args[i].as_str() {
            "-h" | "--help" => {
                println!("{}", HELP);
                exit(0);
            }
            "-f" | "--force" => {
                config.force = true;
            }
            "-l" | "--list" => {
                config.list_only = true;
            }
            "-v" | "--verbose" => {
                config.verbose = true;
            }
            "-q" | "--quiet" => {
                config.quiet = true;
            }
            _ => {
                if let Ok(port) = args[i].parse::<u16>() {
                    config.ports.push(port);
                } else {
                    eprintln!("Error: Invalid port number: {}", args[i]);
                    exit(1);
                }
            }
        }
        i += 1;
    }
    
    if config.ports.is_empty() {
        eprintln!("Error: No ports specified");
        eprintln!("Try 'killport --help' for more information.");
        exit(1);
    }
    
    // Проверяем root права для портов < 1024
    let is_root = Command::new("id")
        .arg("-u")
        .output()
        .map(|output| String::from_utf8_lossy(&output.stdout).trim() == "0")
        .unwrap_or(false);
    
    let needs_root = config.ports.iter().any(|&p| p < 1024);
    if needs_root && !is_root && !config.list_only {
        eprintln!("Error: Root privileges required for ports below 1024");
        exit(1);
    }
    
    let mut port_processes = HashMap::new();
    let mut found = false;
    
    // Собираем информацию о процессах для каждого порта
    for &port in &config.ports {
        let processes = get_processes_by_port(port);
        if !processes.is_empty() {
            found = true;
            port_processes.insert(port, processes);
        }
    }
    
    if !found {
        if !config.quiet {
            println!("No processes found for specified ports");
        }
        exit(0);
    }
    
    // Выводим информацию и/или завершаем процессы
    for (&port, processes) in &port_processes {
        for proc in processes {
            if !config.quiet {
                print_process_info(proc, port, config.verbose);
            }
            
            if !config.list_only {
                if kill_process(proc.pid, config.force) {
                    if !config.quiet {
                        println!("Successfully terminated process {} (PID: {})",
                               proc.name, proc.pid);
                    }
                } else {
                    eprintln!("Failed to terminate process {} (PID: {})",
                            proc.name, proc.pid);
                }
            }
        }
    }
}