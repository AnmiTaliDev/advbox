use std::env;
use std::fs;
use std::io;
use std::path::{Path, PathBuf};

const HELP: &str = r#"
FTree - File System Tree Visualizer

Usage:
    ftree [OPTIONS] [directory]

Options:
    -L, --level <N>    Maximum display depth (default: unlimited)
    -s, --size         Show file sizes
    -h, --hidden       Show hidden files
    -d, --dirs-only    Show directories only
    -p, --pattern <P>  Filter by pattern (e.g., "*.rs")
    -i, --ignore <P>   Ignore pattern (e.g., "target")
    --help            Show this help message

Examples:
    ftree
    ftree -L 2 /path/to/dir
    ftree -s -h src/
    ftree -p "*.rs" -i "target"
"#;

#[derive(Debug)]
struct Config {
    root: PathBuf,
    max_depth: Option<usize>,
    show_size: bool,
    show_hidden: bool,
    dirs_only: bool,
    pattern: Option<String>,
    ignore: Option<String>,
}

#[derive(Debug)]
struct TreeStats {
    total_dirs: usize,
    total_files: usize,
    total_size: u64,
}

impl Default for TreeStats {
    fn default() -> Self {
        TreeStats {
            total_dirs: 0,
            total_files: 0,
            total_size: 0,
        }
    }
}

fn format_size(size: u64) -> String {
    const UNITS: [&str; 6] = ["B", "KB", "MB", "GB", "TB", "PB"];
    let mut size = size as f64;
    let mut unit_index = 0;

    while size >= 1024.0 && unit_index < UNITS.len() - 1 {
        size /= 1024.0;
        unit_index += 1;
    }

    if unit_index == 0 {
        format!("{} {}", size as u64, UNITS[unit_index])
    } else {
        format!("{:.1} {}", size, UNITS[unit_index])
    }
}

fn matches_pattern(name: &str, pattern: &str) -> bool {
    if pattern.starts_with("*.") {
        name.ends_with(&pattern[1..])
    } else {
        name.contains(pattern)
    }
}

fn should_process_file(
    entry: &fs::DirEntry,
    config: &Config,
    is_dir: bool,
) -> bool {
    let name = entry.file_name();
    let name_str = name.to_string_lossy();

    // Hidden file check
    if !config.show_hidden && name_str.starts_with('.') {
        return false;
    }

    // Directories-only check
    if config.dirs_only && !is_dir {
        return false;
    }

    // Include pattern check
    if let Some(ref pattern) = config.pattern {
        if !is_dir && !matches_pattern(&name_str, pattern) {
            return false;
        }
    }

    // Ignore pattern check
    if let Some(ref ignore) = config.ignore {
        if matches_pattern(&name_str, ignore) {
            return false;
        }
    }

    true
}

fn print_tree(
    path: &Path,
    prefix: &str,
    last_item: bool,
    depth: usize,
    config: &Config,
    stats: &mut TreeStats,
    is_root: bool,
) -> io::Result<()> {
    if let Some(max_depth) = config.max_depth {
        if depth > max_depth {
            return Ok(());
        }
    }

    let metadata = fs::metadata(path)?;
    let is_dir = metadata.is_dir();

    if !is_root {
        let marker = if last_item { "└── " } else { "├── " };
        print!("{}{}", prefix, marker);
        
        let name = path.file_name().unwrap_or_default().to_string_lossy();
        print!("{}", name);

        if config.show_size {
            if is_dir {
                print!(" [DIR]");
            } else {
                print!(" [{}]", format_size(metadata.len()));
            }
        }
        println!();
    }

    if is_dir {
        if !is_root {
            stats.total_dirs += 1;
        }

        let mut entries: Vec<_> = fs::read_dir(path)?
            .filter_map(|e| e.ok())
            .filter(|e| should_process_file(e, config, e.path().is_dir()))
            .collect();

        entries.sort_by_key(|e| (e.path().is_file(), e.file_name()));

        let total = entries.len();
        for (index, entry) in entries.into_iter().enumerate() {
            let new_prefix = if is_root {
                String::new()
            } else if last_item {
                format!("{}    ", prefix)
            } else {
                format!("{}│   ", prefix)
            };

            print_tree(
                &entry.path(),
                &new_prefix,
                index == total - 1,
                depth + 1,
                config,
                stats,
                false,
            )?;
        }
    } else {
        stats.total_files += 1;
        stats.total_size += metadata.len();
    }

    Ok(())
}

fn main() -> io::Result<()> {
    let args: Vec<String> = env::args().collect();
    let mut config = Config {
        root: PathBuf::from("."),
        max_depth: None,
        show_size: false,
        show_hidden: false,
        dirs_only: false,
        pattern: None,
        ignore: None,
    };

    let mut i = 1;
    while i < args.len() {
        match args[i].as_str() {
            "--help" => {
                println!("{}", HELP);
                return Ok(());
            }
            "-L" | "--level" => {
                i += 1;
                if i < args.len() {
                    config.max_depth = Some(args[i].parse().unwrap_or(0));
                }
            }
            "-s" | "--size" => {
                config.show_size = true;
            }
            "-h" | "--hidden" => {
                config.show_hidden = true;
            }
            "-d" | "--dirs-only" => {
                config.dirs_only = true;
            }
            "-p" | "--pattern" => {
                i += 1;
                if i < args.len() {
                    config.pattern = Some(args[i].clone());
                }
            }
            "-i" | "--ignore" => {
                i += 1;
                if i < args.len() {
                    config.ignore = Some(args[i].clone());
                }
            }
            _ => {
                if !args[i].starts_with('-') {
                    config.root = PathBuf::from(&args[i]);
                }
            }
        }
        i += 1;
    }

    if !config.root.exists() {
        return Err(io::Error::new(
            io::ErrorKind::NotFound,
            "Directory not found",
        ));
    }

    let mut stats = TreeStats::default();
    println!("{}", config.root.display());
    print_tree(
        &config.root,
        "",
        true,
        0,
        &config,
        &mut stats,
        true,
    )?;

    println!("\nSummary:");
    println!("  {} directories", stats.total_dirs);
    println!("  {} files", stats.total_files);
    if config.show_size {
        println!("  Total size: {}", format_size(stats.total_size));
    }

    Ok(())
}