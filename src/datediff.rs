use std::env;
use std::process;
use std::time::{SystemTime, UNIX_EPOCH};

const HELP: &str = r#"
DateDiff - Date and Time Difference Calculator

Usage:
    datediff [OPTIONS] <date1> [date2]

Options:
    -h, --help          Show this help message
    -n, --now          Use current time as second date
    -u, --unit <unit>  Output unit (years|months|days|hours|minutes|seconds)
    -f, --format       Format output as detailed breakdown
    -s, --simple       Simple output (only numbers)

Date Formats:
    YYYY-MM-DD
    YYYY-MM-DD HH:MM:SS
    HH:MM:SS (today's date is assumed)
    now (current date and time)
    today (current date at 00:00:00)
    yesterday (yesterday at 00:00:00)
    tomorrow (tomorrow at 00:00:00)

Examples:
    datediff "2024-01-01" "2025-01-01"
    datediff -n "2024-01-01"
    datediff -u days "2024-01-01" "2024-02-01"
    datediff -f "2024-01-01 12:00:00" "2024-01-02 15:30:45"
"#;

#[derive(Debug, Clone, Copy)]
struct DateTime {
    year: i32,
    month: u32,
    day: u32,
    hour: u32,
    minute: u32,
    second: u32,
}

impl DateTime {
    fn new(year: i32, month: u32, day: u32, hour: u32, minute: u32, second: u32) -> Self {
        DateTime {
            year,
            month,
            day,
            hour,
            minute,
            second,
        }
    }

    fn from_str(s: &str) -> Result<Self, String> {
        // Обработка специальных ключевых слов
        match s.to_lowercase().as_str() {
            "now" => return Ok(DateTime::now()),
            "today" => return Ok(DateTime::today()),
            "yesterday" => return Ok(DateTime::yesterday()),
            "tomorrow" => return Ok(DateTime::tomorrow()),
            _ => {}
        }

        // Парсинг даты и времени из строки
        let parts: Vec<&str> = s.split(' ').collect();
        let date_parts: Vec<&str> = parts[0].split('-').collect();
        
        if date_parts.len() != 3 {
            return Err("Invalid date format. Expected YYYY-MM-DD".to_string());
        }

        let year = date_parts[0].parse::<i32>()
            .map_err(|_| "Invalid year")?;
        let month = date_parts[1].parse::<u32>()
            .map_err(|_| "Invalid month")?;
        let day = date_parts[2].parse::<u32>()
            .map_err(|_| "Invalid day")?;

        let (hour, minute, second) = if parts.len() > 1 {
            let time_parts: Vec<&str> = parts[1].split(':').collect();
            if time_parts.len() != 3 {
                return Err("Invalid time format. Expected HH:MM:SS".to_string());
            }
            (
                time_parts[0].parse::<u32>().map_err(|_| "Invalid hour")?,
                time_parts[1].parse::<u32>().map_err(|_| "Invalid minute")?,
                time_parts[2].parse::<u32>().map_err(|_| "Invalid second")?,
            )
        } else {
            (0, 0, 0)
        };

        // Проверка валидности
        if month < 1 || month > 12 {
            return Err("Month must be between 1 and 12".to_string());
        }
        if day < 1 || day > 31 {
            return Err("Day must be between 1 and 31".to_string());
        }
        if hour > 23 {
            return Err("Hour must be between 0 and 23".to_string());
        }
        if minute > 59 {
            return Err("Minute must be between 0 and 59".to_string());
        }
        if second > 59 {
            return Err("Second must be between 0 and 59".to_string());
        }

        Ok(DateTime::new(year, month, day, hour, minute, second))
    }

    fn now() -> Self {
        let now = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_secs();

        let (year, month, day, hour, minute, second) = seconds_to_date(now as i64);
        DateTime::new(year, month, day, hour, minute, second)
    }

    fn today() -> Self {
        let now = DateTime::now();
        DateTime::new(now.year, now.month, now.day, 0, 0, 0)
    }

    fn yesterday() -> Self {
        let now = DateTime::today();
        let timestamp = date_to_seconds(now.year, now.month, now.day, 0, 0, 0) - 86400;
        let (year, month, day, _, _, _) = seconds_to_date(timestamp);
        DateTime::new(year, month, day, 0, 0, 0)
    }

    fn tomorrow() -> Self {
        let now = DateTime::today();
        let timestamp = date_to_seconds(now.year, now.month, now.day, 0, 0, 0) + 86400;
        let (year, month, day, _, _, _) = seconds_to_date(timestamp);
        DateTime::new(year, month, day, 0, 0, 0)
    }

    fn to_seconds(&self) -> i64 {
        date_to_seconds(self.year, self.month, self.day, 
                       self.hour, self.minute, self.second)
    }
}

// Конвертация даты в секунды от UNIX эпохи
fn date_to_seconds(year: i32, month: u32, day: u32, 
                  hour: u32, minute: u32, second: u32) -> i64 {
    let days_before_month = [0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334];
    
    let mut years = year - 1970;
    let mut days = days_before_month[month as usize - 1] + day - 1;
    
    // Добавляем дни за високосные годы
    let leap_years = (1968 + years) / 4 - (1968 + years) / 100 + (1968 + years) / 400 
                    - (1968) / 4 + (1968) / 100 - (1968) / 400;
    days += leap_years as u32;
    
    // Проверяем текущий год на високосность
    if month > 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) {
        days += 1;
    }
    
    days as i64 * 86400 + hour as i64 * 3600 + minute as i64 * 60 + second as i64
}

// Конвертация секунд в дату
fn seconds_to_date(secs: i64) -> (i32, u32, u32, u32, u32, u32) {
    let days = secs / 86400;
    let secs_of_day = secs % 86400;
    
    let hour = (secs_of_day / 3600) as u32;
    let minute = ((secs_of_day % 3600) / 60) as u32;
    let second = (secs_of_day % 60) as u32;
    
    let mut year = 1970;
    let mut days_remaining = days;
    
    while days_remaining >= 365 {
        let days_in_year = if (year % 4 == 0 && year % 100 != 0) || year % 400 == 0 {
            366
        } else {
            365
        };
        
        if days_remaining >= days_in_year {
            days_remaining -= days_in_year;
            year += 1;
        } else {
            break;
        }
    }
    
    let month_days = [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
    let mut month = 1;
    let mut day = days_remaining + 1;
    
    // Корректировка для високосного года
    let is_leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    
    for (i, &days_in_month) in month_days.iter().enumerate() {
        let mut dim = days_in_month;
        if i == 1 && is_leap {
            dim += 1;
        }
        
        if day > dim {
            day -= dim;
            month += 1;
        } else {
            break;
        }
    }
    
    (year, month as u32, day as u32, hour, minute, second)
}

#[derive(Debug)]
struct TimeDiff {
    years: i64,
    months: i64,
    days: i64,
    hours: i64,
    minutes: i64,
    seconds: i64,
    total_seconds: i64,
}

fn calculate_diff(date1: DateTime, date2: DateTime) -> TimeDiff {
    let seconds1 = date1.to_seconds();
    let seconds2 = date2.to_seconds();
    let total_seconds = seconds2 - seconds1;
    
    let total_days = total_seconds / 86400;
    let years = total_days / 365;
    let remaining_days = total_days % 365;
    let months = remaining_days / 30;
    let days = remaining_days % 30;
    
    let remaining_seconds = total_seconds % 86400;
    let hours = remaining_seconds / 3600;
    let minutes = (remaining_seconds % 3600) / 60;
    let seconds = remaining_seconds % 60;
    
    TimeDiff {
        years,
        months,
        days,
        hours,
        minutes,
        seconds,
        total_seconds,
    }
}

fn format_diff(diff: &TimeDiff, unit: Option<&str>, format: bool, simple: bool) -> String {
    if simple {
        if let Some(unit) = unit {
            match unit {
                "years" => return format!("{}", diff.total_seconds / (365 * 86400)),
                "months" => return format!("{}", diff.total_seconds / (30 * 86400)),
                "days" => return format!("{}", diff.total_seconds / 86400),
                "hours" => return format!("{}", diff.total_seconds / 3600),
                "minutes" => return format!("{}", diff.total_seconds / 60),
                "seconds" => return format!("{}", diff.total_seconds),
                _ => {}
            }
        }
        return diff.total_seconds.to_string();
    }

    if format {
        let mut parts = Vec::new();
        if diff.years > 0 {
            parts.push(format!("{} years", diff.years));
        }
        if diff.months > 0 {
            parts.push(format!("{} months", diff.months));
        }
        if diff.days > 0 {
            parts.push(format!("{} days", diff.days));
        }
        if diff.hours > 0 {
            parts.push(format!("{} hours", diff.hours));
        }
        if diff.minutes > 0 {
            parts.push(format!("{} minutes", diff.minutes));
        }
        if diff.seconds > 0 {
            parts.push(format!("{} seconds", diff.seconds));
        }
        
        if parts.is_empty() {
            return "0 seconds".to_string();
        }
        return parts.join(", ");
    }

    if let Some(unit) = unit {
        match unit {
            "years" => format!("{:.2} years", diff.total_seconds as f64 / (365.0 * 86400.0)),
            "months" => format!("{:.2} months", diff.total_seconds as f64 / (30.0 * 86400.0)),
            "days" => format!("{:.2} days", diff.total_seconds as f64 / 86400.0),
            "hours" => format!("{:.2} hours", diff.total_seconds as f64 / 3600.0),
            "minutes" => format!("{:.2} minutes", diff.total_seconds as f64 / 60.0),
            "seconds" => format!("{} seconds", diff.total_seconds),
            _ => format!("Invalid unit: {}", unit),
        }
    } else {
        format!("{:.2} days", diff.total_seconds as f64 / 86400.0)
    }
}

fn main() {
    let args: Vec<String> = env::args().collect();
    let mut date1_str = String::new();
    let mut date2_str = String::new();
    let mut use_now = false;
    let mut unit = None;
    let mut format = false;
    let mut simple = false;
    
    let mut i = 1;
    while i < args.len() {
        match args[i].as_str() {
            "-h" | "--help" => {
                println!("{}", HELP);
                return;
            }
            "-n" | "--now" => {
                use_now = true;
                i += 1;
            }
            "-u" | "--unit" => {
                if i + 1 < args.len() {
                    unit = Some(args[i + 1].as_str());
                    i += 2;
                } else {
                    eprintln!("Error: Unit not specified");
                    process::exit(1);
                }
            }
            "-f" | "--format" => {
                format = true;
                i += 1;
            }
            "-s" | "--simple" => {
                simple = true;
                i += 1;
            }
            _ => {
                if date1_str.is_empty() {
                    date1_str = args[i].clone();
                } else if date2_str.is_empty() {
                    date2_str = args[i].clone();
                }
                i += 1;
            }
        }
    }

    if date1_str.is_empty() {
        eprintln!("Error: First date not specified");
        eprintln!("Try 'datediff --help' for more information.");
        process::exit(1);
    }

    if use_now {
        date2_str = "now".to_string();
    }

    if date2_str.is_empty() {
        date2_str = "now".to_string();
    }

    let date1 = match DateTime::from_str(&date1_str) {
        Ok(date) => date,
        Err(e) => {
            eprintln!("Error parsing first date: {}", e);
            process::exit(1);
        }
    };

    let date2 = match DateTime::from_str(&date2_str) {
        Ok(date) => date,
        Err(e) => {
            eprintln!("Error parsing second date: {}", e);
            process::exit(1);
        }
    };
    let diff = calculate_diff(date1, date2);
        println!("{}", format_diff(&diff, unit, format, simple));
}