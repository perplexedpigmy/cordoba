#include <gd/gd.h>
#include <filesystem>
#include <iostream>

int main() {
    auto repo_path = std::filesystem::temp_directory_path() / "cordoba_test_repo";
    std::filesystem::remove_all(repo_path);
    
    // Open or create repository
    auto result = gd::open(repo_path);
    if (!result) {
        result = gd::init(repo_path);
    }
    
    if (!result) {
        std::cerr << "Failed to open/create repository" << std::endl;
        return 1;
    }
    
    std::cout << "Success! Repository opened at: " << repo_path << std::endl;
    std::filesystem::remove_all(repo_path);
    return 0;
}
