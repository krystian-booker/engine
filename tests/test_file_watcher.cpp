#include "core/file_watcher.h"

#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

namespace {

void CreateTestFile(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    file << content;
}

void ModifyTestFile(const std::string& path, const std::string& content) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    std::ofstream file(path);
    file << content;
}

void DeleteTestFile(const std::string& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

void CleanupDirectory(const std::string& path) {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
    if (ec) {
        throw std::runtime_error("Failed to clean directory: " + path + " (" + ec.message() + ")");
    }
}

} // namespace

void TestFileAddition() {
    std::cout << "\n[TEST 1] File Addition Detection:" << std::endl;

    CleanupDirectory("test_assets");
    std::filesystem::create_directory("test_assets");

    FileWatcher watcher;
    bool callbackInvoked = false;
    FileAction detectedAction = FileAction::Added;
    std::string detectedPath;

    watcher.RegisterCallback(".txt", [&](const std::string& path, FileAction action) {
        callbackInvoked = true;
        detectedAction = action;
        detectedPath = path;
    });

    watcher.WatchDirectory("test_assets", false);

    CreateTestFile("test_assets/test1.txt", "Hello");
    watcher.Update();

    assert(callbackInvoked && "Addition callback not invoked");
    assert(detectedAction == FileAction::Added && "Wrong action for addition");
    assert(detectedPath.find("test1.txt") != std::string::npos && "Wrong file reported");
    std::cout << "  ✓ File addition detected successfully" << std::endl;

    watcher.UnwatchDirectory("test_assets");
    DeleteTestFile("test_assets/test1.txt");
    CleanupDirectory("test_assets");
}

void TestFileModification() {
    std::cout << "\n[TEST 2] File Modification Detection:" << std::endl;

    CleanupDirectory("test_assets");
    std::filesystem::create_directory("test_assets");
    CreateTestFile("test_assets/test2.txt", "Initial");

    FileWatcher watcher;
    int callbackCount = 0;
    FileAction lastAction = FileAction::Added;

    watcher.RegisterCallback(".txt", [&](const std::string&, FileAction action) {
        callbackCount++;
        lastAction = action;
    });

    watcher.WatchDirectory("test_assets", false);

    watcher.Update();
    assert(callbackCount == 1 && lastAction == FileAction::Added);

    ModifyTestFile("test_assets/test2.txt", "Modified");
    watcher.Update();

    assert(callbackCount == 2 && lastAction == FileAction::Modified);
    std::cout << "  ✓ File modification detected successfully" << std::endl;

    watcher.UnwatchDirectory("test_assets");
    DeleteTestFile("test_assets/test2.txt");
    CleanupDirectory("test_assets");
}

void TestFileDeletion() {
    std::cout << "\n[TEST 3] File Deletion Detection:" << std::endl;

    CleanupDirectory("test_assets");
    std::filesystem::create_directory("test_assets");
    CreateTestFile("test_assets/test3.txt", "Delete me");

    FileWatcher watcher;
    int callbackCount = 0;
    FileAction lastAction = FileAction::Added;

    watcher.RegisterCallback(".txt", [&](const std::string&, FileAction action) {
        callbackCount++;
        lastAction = action;
    });

    watcher.WatchDirectory("test_assets", false);

    watcher.Update();
    assert(callbackCount == 1 && lastAction == FileAction::Added);

    DeleteTestFile("test_assets/test3.txt");
    watcher.Update();

    assert(callbackCount == 2 && lastAction == FileAction::Deleted);
    std::cout << "  ✓ File deletion detected successfully" << std::endl;

    watcher.UnwatchDirectory("test_assets");
    CleanupDirectory("test_assets");
}

void TestMultipleCallbacks() {
    std::cout << "\n[TEST 4] Multiple Callbacks:" << std::endl;

    CleanupDirectory("test_assets");
    std::filesystem::create_directory("test_assets");

    FileWatcher watcher;
    int callbackA = 0;
    int callbackB = 0;

    watcher.RegisterCallback(".txt", [&](const std::string&, FileAction) { callbackA++; });
    watcher.RegisterCallback(".txt", [&](const std::string&, FileAction) { callbackB++; });

    watcher.WatchDirectory("test_assets", false);

    CreateTestFile("test_assets/test4.txt", "Callbacks");
    watcher.Update();

    assert(callbackA == 1 && callbackB == 1);
    std::cout << "  ✓ Multiple callbacks invoked" << std::endl;

    watcher.UnwatchDirectory("test_assets");
    DeleteTestFile("test_assets/test4.txt");
    CleanupDirectory("test_assets");
}

void TestRecursiveWatching() {
    std::cout << "\n[TEST 5] Recursive Watching:" << std::endl;

    CleanupDirectory("test_assets");
    std::filesystem::create_directories("test_assets/subdir");

    FileWatcher watcher;
    int rootCount = 0;
    int subCount = 0;

    watcher.RegisterCallback(".txt", [&](const std::string& path, FileAction) {
        if (path.find("subdir") != std::string::npos) {
            subCount++;
        } else {
            rootCount++;
        }
    });

    watcher.WatchDirectory("test_assets", true);

    CreateTestFile("test_assets/root.txt", "Root");
    CreateTestFile("test_assets/subdir/child.txt", "Child");
    watcher.Update();

    assert(rootCount == 1 && subCount == 1);
    std::cout << "  ✓ Recursive watching works" << std::endl;

    watcher.UnwatchDirectory("test_assets");
    DeleteTestFile("test_assets/root.txt");
    DeleteTestFile("test_assets/subdir/child.txt");
    CleanupDirectory("test_assets");
}

void TestExtensionFiltering() {
    std::cout << "\n[TEST 6] Extension Filtering:" << std::endl;

    CleanupDirectory("test_assets");
    std::filesystem::create_directory("test_assets");

    FileWatcher watcher;
    int txtCount = 0;
    int jsonCount = 0;

    watcher.RegisterCallback(".txt", [&](const std::string&, FileAction) { txtCount++; });
    watcher.RegisterCallback(".json", [&](const std::string&, FileAction) { jsonCount++; });

    watcher.WatchDirectory("test_assets", false);

    CreateTestFile("test_assets/file.txt", "Text");
    CreateTestFile("test_assets/data.json", "{}");
    CreateTestFile("test_assets/notes.md", "Ignore");
    watcher.Update();

    assert(txtCount == 1 && jsonCount == 1);
    std::cout << "  ✓ Extension filtering works" << std::endl;

    watcher.UnwatchDirectory("test_assets");
    DeleteTestFile("test_assets/file.txt");
    DeleteTestFile("test_assets/data.json");
    DeleteTestFile("test_assets/notes.md");
    CleanupDirectory("test_assets");
}

void TestUnwatchDirectory() {
    std::cout << "\n[TEST 7] Unwatch Directory:" << std::endl;

    CleanupDirectory("test_assets");
    std::filesystem::create_directory("test_assets");

    FileWatcher watcher;
    int callbackCount = 0;

    watcher.RegisterCallback(".txt", [&](const std::string&, FileAction) { callbackCount++; });
    watcher.WatchDirectory("test_assets", false);

    CreateTestFile("test_assets/test7.txt", "First");
    watcher.Update();
    assert(callbackCount == 1);

    watcher.UnwatchDirectory("test_assets");
    CreateTestFile("test_assets/test8.txt", "Second");
    watcher.Update();
    assert(callbackCount == 1);
    std::cout << "  ✓ Unwatch disables notifications" << std::endl;

    DeleteTestFile("test_assets/test7.txt");
    DeleteTestFile("test_assets/test8.txt");
    CleanupDirectory("test_assets");
}

int main() {
    std::cout << "=== FileWatcher Tests ===" << std::endl;

    try {
        TestFileAddition();
        TestFileModification();
        TestFileDeletion();
        TestMultipleCallbacks();
        TestRecursiveWatching();
        TestExtensionFiltering();
        TestUnwatchDirectory();

        std::cout << "\n===============================================" << std::endl;
        std::cout << "All FileWatcher tests passed! :)" << std::endl;
        std::cout << "===============================================" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n[ERROR] Test failed: " << e.what() << std::endl;
        return 1;
    }
}
