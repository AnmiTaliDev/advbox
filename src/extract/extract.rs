use std::env;
use std::path::{Path, PathBuf};
use std::process::{Command, exit};
use std::fs;

const HELP: &str = r#"
Extract - Universal archive extractor

Usage:
    extract [OPTIONS] <archive> [destination]

Options:
    -l, --list       List contents without extracting
    -f, --force      Overwrite existing files
    -q, --quiet      Suppress output except errors
    -k, --keep       Keep archive after extraction
    -h, --help       Show this help message

Supported formats:
    .zip, .tar, .tar.gz, .tgz, .tar.bz2, .tbz2,
    .tar.xz, .txz, .tar.zst, .7z, .rar

Examples:
    extract archive.zip
    extract -l backup.tar.gz
    extract data.7z /path/to/dest
"#;

#[derive(Debug)]
struct Config {
    archive_path: PathBuf,
    destination: Option<PathBuf>,
    list_only: bool,
    force: bool,
    quiet: bool,
    keep: bool,
}

#[derive(Debug)]
enum ArchiveType {
    Zip,
    Tar,
    TarGz,
    TarBz2,
    TarXz,
    TarZst,
    SevenZip,
    Rar,
    Unknown,
}

impl ArchiveType {
    fn from_path(path: &Path) -> Self {
        let ext = path.extension()
            .and_then(|e| e.to_str())
            .unwrap_or("");
        
        let name = path.file_name()
            .and_then(|n| n.to_str())
            .unwrap_or("");
        
        match (name.to_lowercase().as_str(), ext.to_lowercase().as_str()) {
            (n, _) if n.ends_with(".tar.gz") => ArchiveType::TarGz,
            (n, _) if n.ends_with(".tar.bz2") => ArchiveType::TarBz2,
            (n, _) if n.ends_with(".tar.xz") => ArchiveType::TarXz,
            (n, _) if n.ends_with(".tar.zst") => ArchiveType::TarZst,
            (_, "tgz") => ArchiveType::TarGz,
            (_, "tbz2") => ArchiveType::TarBz2,
            (_, "txz") => ArchiveType::TarXz,
            (_, "zip") => ArchiveType::Zip,
            (_, "tar") => ArchiveType::Tar,
            (_, "7z") => ArchiveType::SevenZip,
            (_, "rar") => ArchiveType::Rar,
            _ => ArchiveType::Unknown,
        }
    }
    
    fn get_command(&self) -> Option<(&'static str, Vec<&'static str>)> {
        match self {
            ArchiveType::Zip => Some(("unzip", vec!["-q"])),
            ArchiveType::Tar => Some(("tar", vec!["-xf"])),
            ArchiveType::TarGz => Some(("tar", vec!["-xzf"])),
            ArchiveType::TarBz2 => Some(("tar", vec!["-xjf"])),
            ArchiveType::TarXz => Some(("tar", vec!["-xJf"])),
            ArchiveType::TarZst => Some(("tar", vec!["--zstd", "-xf"])),
            ArchiveType::SevenZip => Some(("7z", vec!["x"])),
            ArchiveType::Rar => Some(("unrar", vec!["x"])),
            ArchiveType::Unknown => None,
        }
    }
    
    fn get_list_command(&self) -> Option<(&'static str, Vec<&'static str>)> {
        match self {
            ArchiveType::Zip => Some(("unzip", vec!["-l"])),
            ArchiveType::Tar => Some(("tar", vec!["-tf"])),
            ArchiveType::TarGz => Some(("tar", vec!["-tzf"])),
            ArchiveType::TarBz2 => Some(("tar", vec!["-tjf"])),
            ArchiveType::TarXz => Some(("tar", vec!["-tJf"])),
            ArchiveType::TarZst => Some(("tar", vec!["--zstd", "-tf"])),
            ArchiveType::SevenZip => Some(("7z", vec!["l"])),
            ArchiveType::Rar => Some(("unrar", vec!["l"])),
            ArchiveType::Unknown => None,
        }
    }
}

fn check_command_exists(command: &str) -> bool {
    Command::new("which")
        .arg(command)
        .output()
        .map(|output| output.status.success())
        .unwrap_or(false)
}

fn extract_archive(config: &Config) -> Result<(), String> {
    let archive_type = ArchiveType::from_path(&config.archive_path);
    
    match archive_type {
        ArchiveType::Unknown => {
            return Err("Unsupported archive format".to_string());
        }
        _ => {
            if let Some((cmd, base_args)) = if config.list_only {
                archive_type.get_list_command()
            } else {
                archive_type.get_command()
            } {
                if !check_command_exists(cmd) {
                    return Err(format!("Required command '{}' not found", cmd));
                }
                
                let mut command = Command::new(cmd);
                command.args(base_args);
                
                // Add format-specific options
                match cmd {
                    "unzip" => {
                        if config.force {
                            command.arg("-o");
                        }
                        if config.quiet {
                            command.arg("-qq");
                        }
                    }
                    "7z" => {
                        if config.quiet {
                            command.arg("-bd");
                        }
                        if config.force {
                            command.arg("-y");
                        }
                    }
                    "unrar" => {
                        if config.force {
                            command.arg("-o+");
                        }
                        if config.quiet {
                            command.arg("-inul");
                        }
                    }
                    _ => {}
                }
                
                command.arg(&config.archive_path);
                
                if !config.list_only {
                    if let Some(ref dest) = config.destination {
                        // Create the destination directory if it does not exist
                        fs::create_dir_all(dest)
                            .map_err(|e| format!("Failed to create destination directory: {}", e))?;
                        
                        command.current_dir(dest);
                    }
                }
                
                let output = command
                    .output()
                    .map_err(|e| format!("Failed to execute command: {}", e))?;
                
                if !output.status.success() {
                    return Err(format!("Extraction failed: {}", 
                        String::from_utf8_lossy(&output.stderr)));
                }
                
                if !config.quiet {
                    println!("{}", String::from_utf8_lossy(&output.stdout));
                }
                
                // Remove the archive unless the keep flag is set
                if !config.keep && !config.list_only {
                    fs::remove_file(&config.archive_path)
                        .map_err(|e| format!("Failed to remove archive: {}", e))?;
                }
                
                Ok(())
            } else {
                Err("Internal error: command not found for archive type".to_string())
            }
        }
    }
}

fn main() {
    let args: Vec<String> = env::args().collect();
    let mut config = Config {
        archive_path: PathBuf::new(),
        destination: None,
        list_only: false,
        force: false,
        quiet: false,
        keep: false,
    };
    
    let mut i = 1;
    while i < args.len() {
        match args[i].as_str() {
            "-h" | "--help" => {
                println!("{}", HELP);
                exit(0);
            }
            "-l" | "--list" => {
                config.list_only = true;
            }
            "-f" | "--force" => {
                config.force = true;
            }
            "-q" | "--quiet" => {
                config.quiet = true;
            }
            "-k" | "--keep" => {
                config.keep = true;
            }
            _ => {
                if config.archive_path.as_os_str().is_empty() {
                    config.archive_path = PathBuf::from(&args[i]);
                } else {
                    config.destination = Some(PathBuf::from(&args[i]));
                }
            }
        }
        i += 1;
    }
    
    if config.archive_path.as_os_str().is_empty() {
        eprintln!("Error: No archive specified");
        eprintln!("Try 'extract --help' for more information.");
        exit(1);
    }
    
    if !config.archive_path.exists() {
        eprintln!("Error: Archive file not found: {}", 
            config.archive_path.display());
        exit(1);
    }
    
    match extract_archive(&config) {
        Ok(_) => {
            if !config.quiet && !config.list_only {
                println!("Extraction completed successfully.");
            }
        }
        Err(e) => {
            eprintln!("Error: {}", e);
            exit(1);
        }
    }
}