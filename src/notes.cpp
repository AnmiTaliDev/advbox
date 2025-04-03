#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <ctime>
#include <iomanip>
#include <filesystem>
#include <algorithm>
#include <sstream>

namespace fs = std::filesystem;

const char* HELP = R"(
Notes - Simple Note Taking Tool

Usage:
    notes [OPTIONS] [TEXT]
    notes add "Note text"      Create a new note
    notes list                 List all notes
    notes show <id>           Show note content
    notes edit <id>           Edit note
    notes rm <id>             Remove note
    notes search <text>       Search in notes
    notes tag <id> <tags>     Add tags to note
    notes tags                List all tags
    notes export              Export notes to file

Options:
    -h, --help               Show this help message
    -t, --tags <tags>        Add tags when creating note
    -d, --date <date>        Set custom date (YYYY-MM-DD)
    -f, --format <format>    Output format (text/json)
    --no-color               Disable colored output

Examples:
    notes add "Meeting with John tomorrow"
    notes add "Buy milk" -t shopping,todo
    notes list --tags todo
    notes search "meeting"
    notes edit 1
    notes rm 2
    notes export > backup.json
)";

struct Note {
    int id;
    std::string text;
    std::string date;
    std::vector<std::string> tags;
};

class NotesManager {
private:
    const std::string NOTES_DIR = std::string(getenv("HOME")) + "/.notes";
    const std::string DB_FILE = NOTES_DIR + "/notes.db";
    std::vector<Note> notes;

    void ensure_notes_dir() {
        if (!fs::exists(NOTES_DIR)) {
            fs::create_directory(NOTES_DIR);
        }
    }

    std::string get_current_date() {
        auto now = std::time(nullptr);
        auto tm = *std::localtime(&now);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    void load_notes() {
        notes.clear();
        std::ifstream file(DB_FILE);
        if (!file) return;

        std::string line;
        Note current_note;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            
            if (line[0] == '#') { // Новая заметка
                if (current_note.id != 0) {
                    notes.push_back(current_note);
                }
                current_note = Note();
                current_note.id = std::stoi(line.substr(1));
            }
            else if (line[0] == '@') { // Дата
                current_note.date = line.substr(1);
            }
            else if (line[0] == '*') { // Теги
                std::string tags = line.substr(1);
                std::stringstream ss(tags);
                std::string tag;
                while (std::getline(ss, tag, ',')) {
                    current_note.tags.push_back(tag);
                }
            }
            else { // Текст заметки
                if (!current_note.text.empty()) {
                    current_note.text += "\n";
                }
                current_note.text += line;
            }
        }
        
        if (current_note.id != 0) {
            notes.push_back(current_note);
        }
    }

    void save_notes() {
        std::ofstream file(DB_FILE);
        for (const auto& note : notes) {
            file << "#" << note.id << "\n";
            file << "@" << note.date << "\n";
            if (!note.tags.empty()) {
                file << "*";
                for (size_t i = 0; i < note.tags.size(); ++i) {
                    if (i > 0) file << ",";
                    file << note.tags[i];
                }
                file << "\n";
            }
            file << note.text << "\n\n";
        }
    }

    int get_next_id() {
        if (notes.empty()) return 1;
        return notes.back().id + 1;
    }

public:
    NotesManager() {
        ensure_notes_dir();
        load_notes();
    }

    void add_note(const std::string& text, const std::vector<std::string>& tags = {}) {
        Note note;
        note.id = get_next_id();
        note.text = text;
        note.date = get_current_date();
        note.tags = tags;
        notes.push_back(note);
        save_notes();
        std::cout << "Note added with ID: " << note.id << "\n";
    }

    void list_notes(const std::string& tag = "") {
        if (notes.empty()) {
            std::cout << "No notes found.\n";
            return;
        }

        for (const auto& note : notes) {
            if (!tag.empty() && 
                std::find(note.tags.begin(), note.tags.end(), tag) == note.tags.end()) {
                continue;
            }

            std::cout << "\033[1;32m#" << note.id << "\033[0m ";
            std::cout << "\033[1;34m[" << note.date << "]\033[0m ";
            
            if (!note.tags.empty()) {
                std::cout << "\033[1;33m";
                for (const auto& tag : note.tags) {
                    std::cout << "#" << tag << " ";
                }
                std::cout << "\033[0m";
            }
            
            std::cout << "\n";
            
            // Показываем первые 50 символов заметки
            std::string preview = note.text.substr(0, 50);
            if (note.text.length() > 50) {
                preview += "...";
            }
            std::cout << preview << "\n\n";
        }
    }

    void show_note(int id) {
        auto it = std::find_if(notes.begin(), notes.end(),
            [id](const Note& n) { return n.id == id; });
        
        if (it == notes.end()) {
            throw std::runtime_error("Note not found");
        }

        std::cout << "\033[1;32m#" << it->id << "\033[0m ";
        std::cout << "\033[1;34m[" << it->date << "]\033[0m\n";
        
        if (!it->tags.empty()) {
            std::cout << "\033[1;33m";
            for (const auto& tag : it->tags) {
                std::cout << "#" << tag << " ";
            }
            std::cout << "\033[0m\n";
        }
        
        std::cout << "\n" << it->text << "\n";
    }

    void edit_note(int id, const std::string& new_text) {
        auto it = std::find_if(notes.begin(), notes.end(),
            [id](const Note& n) { return n.id == id; });
        
        if (it == notes.end()) {
            throw std::runtime_error("Note not found");
        }

        it->text = new_text;
        it->date = get_current_date() + " (edited)";
        save_notes();
        std::cout << "Note updated.\n";
    }

    void remove_note(int id) {
        auto it = std::find_if(notes.begin(), notes.end(),
            [id](const Note& n) { return n.id == id; });
        
        if (it == notes.end()) {
            throw std::runtime_error("Note not found");
        }

        notes.erase(it);
        save_notes();
        std::cout << "Note removed.\n";
    }

    void search_notes(const std::string& query) {
        bool found = false;
        for (const auto& note : notes) {
            if (note.text.find(query) != std::string::npos) {
                if (!found) {
                    std::cout << "Search results:\n\n";
                    found = true;
                }
                show_note(note.id);
                std::cout << "\n";
            }
        }
        
        if (!found) {
            std::cout << "No matching notes found.\n";
        }
    }

    void add_tags(int id, const std::vector<std::string>& new_tags) {
        auto it = std::find_if(notes.begin(), notes.end(),
            [id](const Note& n) { return n.id == id; });
        
        if (it == notes.end()) {
            throw std::runtime_error("Note not found");
        }

        for (const auto& tag : new_tags) {
            if (std::find(it->tags.begin(), it->tags.end(), tag) == it->tags.end()) {
                it->tags.push_back(tag);
            }
        }
        save_notes();
        std::cout << "Tags added.\n";
    }

    void list_tags() {
        std::vector<std::string> all_tags;
        for (const auto& note : notes) {
            all_tags.insert(all_tags.end(), note.tags.begin(), note.tags.end());
        }
        
        std::sort(all_tags.begin(), all_tags.end());
        all_tags.erase(std::unique(all_tags.begin(), all_tags.end()), all_tags.end());

        if (all_tags.empty()) {
            std::cout << "No tags found.\n";
            return;
        }

        std::cout << "Available tags:\n";
        for (const auto& tag : all_tags) {
            size_t count = std::count_if(notes.begin(), notes.end(),
                [&tag](const Note& n) {
                    return std::find(n.tags.begin(), n.tags.end(), tag) != n.tags.end();
                });
            std::cout << "\033[1;33m#" << tag << "\033[0m (" << count << ")\n";
        }
    }

    void export_notes(const std::string& format = "text") {
        if (format == "json") {
            std::cout << "{\n  \"notes\": [\n";
            for (size_t i = 0; i < notes.size(); ++i) {
                const auto& note = notes[i];
                std::cout << "    {\n";
                std::cout << "      \"id\": " << note.id << ",\n";
                std::cout << "      \"date\": \"" << note.date << "\",\n";
                std::cout << "      \"text\": \"" << note.text << "\",\n";
                std::cout << "      \"tags\": [";
                for (size_t j = 0; j < note.tags.size(); ++j) {
                    if (j > 0) std::cout << ", ";
                    std::cout << "\"" << note.tags[j] << "\"";
                }
                std::cout << "]\n    }";
                if (i < notes.size() - 1) std::cout << ",";
                std::cout << "\n";
            }
            std::cout << "  ]\n}\n";
        } else {
            for (const auto& note : notes) {
                std::cout << "--- Note #" << note.id << " ---\n";
                std::cout << "Date: " << note.date << "\n";
                if (!note.tags.empty()) {
                    std::cout << "Tags: ";
                    for (size_t i = 0; i < note.tags.size(); ++i) {
                        if (i > 0) std::cout << ", ";
                        std::cout << note.tags[i];
                    }
                    std::cout << "\n";
                }
                std::cout << "\n" << note.text << "\n\n";
            }
        }
    }
};

std::vector<std::string> split_tags(const std::string& tags_str) {
    std::vector<std::string> tags;
    std::stringstream ss(tags_str);
    std::string tag;
    while (std::getline(ss, tag, ',')) {
        if (!tag.empty()) {
            tags.push_back(tag);
        }
    }
    return tags;
}

int main(int argc, char* argv[]) {
    try {
        std::vector<std::string> args(argv + 1, argv + argc);
        
        if (args.empty() || args[0] == "-h" || args[0] == "--help") {
            std::cout << HELP;
            return 0;
        }

        NotesManager manager;
        std::string command = args[0];

        if (command == "add") {
            if (args.size() < 2) {
                throw std::runtime_error("Note text required");
            }
            std::vector<std::string> tags;
            for (size_t i = 2; i < args.size(); i++) {
                if (args[i] == "-t" || args[i] == "--tags") {
                    if (++i < args.size()) {
                        tags = split_tags(args[i]);
                    }
                }
            }
            manager.add_note(args[1], tags);
        }
        else if (command == "list") {
            std::string tag;
            for (size_t i = 1; i < args.size(); i++) {
                if (args[i] == "--tags" && ++i < args.size()) {
                    tag = args[i];
                }
            }
            manager.list_notes(tag);
        }
        else if (command == "show") {
            if (args.size() < 2) {
                throw std::runtime_error("Note ID required");
            }
            manager.show_note(std::stoi(args[1]));
        }
        else if (command == "edit") {
            if (args.size() < 3) {
                throw std::runtime_error("Note ID and new text required");
            }
            manager.edit_note(std::stoi(args[1]), args[2]);
        }
        else if (command == "rm") {
            if (args.size() < 2) {
                throw std::runtime_error("Note ID required");
            }
            manager.remove_note(std::stoi(args[1]));
        }
        else if (command == "search") {
            if (args.size() < 2) {
                throw std::runtime_error("Search query required");
            }
            manager.search_notes(args[1]);
        }
        else if (command == "tag") {
            if (args.size() < 3) {
                throw std::runtime_error("Note ID and tags required");
            }
            manager.add_tags(std::stoi(args[1]), split_tags(args[2]));
        }
        else if (command == "tags") {
            manager.list_tags();
        }
        else if (command == "export") {
            std::string format = "text";
            for (size_t i = 1; i < args.size(); i++) {
                if (args[i] == "-f" || args[i] == "--format") {
                    if (++i < args.size()) {
                        format = args[i];
                    }
                }
            }
            manager.export_notes(format);
        }
        else {
            throw std::runtime_error("Unknown command: " + command);
        }

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n'; std::cerr << "Try 'notes --help' for more information.\n"; return 1; } }