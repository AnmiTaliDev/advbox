use std::io::{self, Write};
use std::env;
use std::process;

const HELP: &str = "\
Terminal Colors Utility

Usage: colors [OPTIONS]
Display terminal colors and formatting options

Options:
    -b, --basic     Show basic colors (0-7)
    -e, --extended  Show extended colors (8-15)
    -2, --256       Show 256 color palette
    -r, --rgb       Show RGB color examples
    -f, --format    Show text formatting options
    -t, --test     'Hello World' in different styles
    -h, --help      Show this help message

Example:
    colors -b -f    Show basic colors and formatting
    colors -2       Show 256 color palette
    colors --test   Show test patterns
";

struct Config {
    show_basic: bool,
    show_extended: bool,
    show_256: bool,
    show_rgb: bool,
    show_format: bool,
    show_test: bool,
}

impl Default for Config {
    fn default() -> Self {
        Config {
            show_basic: false,
            show_extended: false,
            show_256: false,
            show_rgb: false,
            show_format: false,
            show_test: false,
        }
    }
}

fn print_header(title: &str) {
    println!("\n{}\n{}", title, "=".repeat(title.len()));
}

fn show_basic_colors() {
    print_header("Basic Colors (0-7)");
    
    // Foreground colors
    print!("Foreground: ");
    for i in 30..38 {
        print!("\x1b[{}m {:02} \x1b[0m", i, i-30);
    }
    println!();
    
    // Background colors
    print!("Background: ");
    for i in 40..48 {
        print!("\x1b[{}m {:02} \x1b[0m", i, i-40);
    }
    println!();
}

fn show_extended_colors() {
    print_header("Extended Colors (8-15)");
    
    // Foreground colors
    print!("Foreground: ");
    for i in 90..98 {
        print!("\x1b[{}m {:02} \x1b[0m", i, i-90);
    }
    println!();
    
    // Background colors
    print!("Background: ");
    for i in 100..108 {
        print!("\x1b[{}m {:02} \x1b[0m", i, i-100);
    }
    println!();
}

fn show_256_colors() {
    print_header("256 Color Mode");
    
    // Standard colors (0-15)
    println!("Standard colors:");
    for i in 0..16 {
        print!("\x1b[48;5;{}m {:3} \x1b[0m", i, i);
        if (i + 1) % 8 == 0 { println!(); }
    }
    
    // Color cube (16-231)
    println!("\nColor cube:");
    for i in 0..6 {
        for j in 0..6 {
            for k in 0..6 {
                let color = 16 + (36 * i) + (6 * j) + k;
                print!("\x1b[48;5;{}m {:3} \x1b[0m", color, color);
            }
            print!(" ");
        }
        println!();
    }
    
    // Grayscale (232-255)
    println!("\nGrayscale:");
    for i in 232..256 {
        print!("\x1b[48;5;{}m {:3} \x1b[0m", i, i);
        if (i + 1) % 8 == 0 { println!(); }
    }
    println!();
}

fn show_rgb_colors() {
    print_header("RGB Color Examples");
    
    // RGB color gradients
    println!("Red gradient:");
    for i in 0..8 {
        let val = i * 31;
        print!("\x1b[48;2;{};0;0m {:3} \x1b[0m", val, val);
    }
    println!();
    
    println!("Green gradient:");
    for i in 0..8 {
        let val = i * 31;
        print!("\x1b[48;2;0;{};0m {:3} \x1b[0m", val, val);
    }
    println!();
    
    println!("Blue gradient:");
    for i in 0..8 {
        let val = i * 31;
        print!("\x1b[48;2;0;0;{}m {:3} \x1b[0m", val, val);
    }
    println!();
    
    // Some predefined RGB colors
    println!("\nSome RGB colors:");
    let colors = [
        (255, 0, 0, "Red"),
        (0, 255, 0, "Green"),
        (0, 0, 255, "Blue"),
        (255, 255, 0, "Yellow"),
        (255, 0, 255, "Magenta"),
        (0, 255, 255, "Cyan"),
    ];
    
    for (r, g, b, name) in colors.iter() {
        print!("\x1b[48;2;{};{};{}m {} \x1b[0m ", r, g, b, name);
    }
    println!();
}

fn show_formatting() {
    print_header("Text Formatting");
    
    let formats = [
        (1, "Bold"),
        (2, "Dim"),
        (3, "Italic"),
        (4, "Underline"),
        (5, "Blink"),
        (7, "Reverse"),
        (9, "Strikethrough"),
    ];
    
    for (code, name) in formats.iter() {
        println!("\x1b[{}m{:<15}\x1b[0m - \\x1b[{}m", code, name, code);
    }
}

fn show_test_patterns() {
    print_header("Test Patterns");
    
    let text = "Hello, World!";
    
    // Different styles
    println!("Normal:          {}", text);
    println!("Bold:            \x1b[1m{}\x1b[0m", text);
    println!("Dim:             \x1b[2m{}\x1b[0m", text);
    println!("Italic:          \x1b[3m{}\x1b[0m", text);
    println!("Underline:       \x1b[4m{}\x1b[0m", text);
    println!("Blink:           \x1b[5m{}\x1b[0m", text);
    println!("Reverse:         \x1b[7m{}\x1b[0m", text);
    println!("Hidden:          \x1b[8m{}\x1b[0m (hidden)", text);
    println!("Strikethrough:   \x1b[9m{}\x1b[0m", text);
    
    // Color combinations
    println!("\nColor combinations:");
    println!("Red on White:    \x1b[31;47m{}\x1b[0m", text);
    println!("Blue on Yellow:  \x1b[34;43m{}\x1b[0m", text);
    println!("White on Blue:   \x1b[37;44m{}\x1b[0m", text);
    println!("Yellow on Red:   \x1b[33;41m{}\x1b[0m", text);
}

fn parse_args() -> Config {
    let args: Vec<String> = env::args().collect();
    let mut config = Config::default();
    
    // Если нет аргументов, показываем все
    if args.len() == 1 {
        config.show_basic = true;
        config.show_extended = true;
        config.show_format = true;
        return config;
    }
    
    for arg in args.iter().skip(1) {
        match arg.as_str() {
            "-b" | "--basic" => config.show_basic = true,
            "-e" | "--extended" => config.show_extended = true,
            "-2" | "--256" => config.show_256 = true,
            "-r" | "--rgb" => config.show_rgb = true,
            "-f" | "--format" => config.show_format = true,
            "-t" | "--test" => config.show_test = true,
            "-h" | "--help" => {
                println!("{}", HELP);
                process::exit(0);
            }
            _ => {
                eprintln!("Unknown option: {}", arg);
                eprintln!("Try 'colors --help' for more information.");
                process::exit(1);
            }
        }
    }
    
    config
}

fn main() {
    let config = parse_args();
    
    if config.show_basic {
        show_basic_colors();
    }
    
    if config.show_extended {
        show_extended_colors();
    }
    
    if config.show_256 {
        show_256_colors();
    }
    
    if config.show_rgb {
        show_rgb_colors();
    }
    
    if config.show_format {
        show_formatting();
    }
    
    if config.show_test {
        show_test_patterns();
    }
    
    // Убедимся, что все цвета сброшены
    print!("\x1b[0m");
    io::stdout().flush().unwrap();
}