#include "core/file_watcher.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <cassert>

// Test helper functions
void CreateTestFile(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    file << content;
    file.close();
}

void ModifyTestFile(const std::string& path, const std::string& content) {
    // Delay to ensure timestamp changes (Windows NTFS has ~10ms resolution, but we use 1s to be safe)
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::ofstream file(path);
    file << content;
    file.close();
}

void DeleteTestFile(const std::string& path) {
    fs::remove(path);
}

// Test 1: File Addition Detection
void TestFileAddition() {
    std::cout << "\n[TEST 1] File Addition Detection:" << std::endl;

    FileWatcher watcher;
    bool callbackInvoked = false;
    std::string detectedPath;
    FileAction detectedAction;

    // Create test directory
    fs::create_directory("test_assets");

    // Register callback
    watcher.RegisterCallback(".txt", [&](const std::string& path, FileAction action) {
        callbackInvoked = true;
        detectedPath = path;
        detectedAction = action;
    });

    // Watch directory
    watcher.WatchDirectory("test_assets", false);

    // Create a new file
    CreateTestFile("test_assets/test1.txt", "Hello World");

    // Update watcher to detect the new file
    watcher.Update();

    // Verify callback was invoked
    assert(callbackInvoked && "Callback should be invoked for file addition");
    assert(detectedAction == FileAction::Added && "Action should be Added");
    std::cout << "  ✓ File addition detected successfully" << std::endl;

    // Cleanup
    DeleteTestFile("test_assets/test1.txt");
    fs::remove("test_assets");
}

// Test 2: File Modification Detection
void TestFileModification() {
    std::cout << "\n[TEST 2] File Modification Detection:" << std::endl;

    FileWatcher watcher;
    int callbackCount = 0;
    FileAction lastAction = FileAction::Added;

    // Create test directory and file
    fs::create_directory("test_assets");
    CreateTestFile("test_assets/test2.txt", "Initial content");

    // Register callback
    watcher.RegisterCallback(".txt", [&](const std::string& path, FileAction action) {
        callbackCount++;
        lastAction = action;
        std::cout << "  Callback invoked: " << path << " (" << static_cast<int>(action) << ")" << std::endl;
    });

    // Watch directory
    watcher.WatchDirectory("test_assets", false);

    // First update - should detect the existing file as "Added"
    watcher.Update();
    assert(callbackCount == 1 && "Should detect file on first scan");
    assert(lastAction == FileAction::Added && "First detection should be Added");

    // Modify the file
    ModifyTestFile("test_assets/test2.txt", "Modified content");

    // Second update - should detect modification
    watcher.Update();
    assert(callbackCount == 2 && "Should detect file modification");
    assert(lastAction == FileAction::Modified && "Action should be Modified");
    std::cout << "  ✓ File modification detected successfully" << std::endl;

    // Cleanup
    DeleteTestFile("test_assets/test2.txt");
    fs::remove("test_assets");
}

// Test 3: File Deletion Detection
void TestFileDeletion() {
    std::cout << "\n[TEST 3] File Deletion Detection:" << std::endl;

    FileWatcher watcher;
    int callbackCount = 0;
    FileAction lastAction = FileAction::Added;

    // Create test directory and file
    fs::create_directory("test_assets");
    CreateTestFile("test_assets/test3.txt", "Content");

    // Register callback
    watcher.RegisterCallback(".txt", [&](const std::string& path, FileAction action) {
        callbackCount++;
        lastAction = action;
        std::cout << "  Callback invoked: " << path << " (" << static_cast<int>(action) << ")" << std::endl;
    });

    // Watch directory
    watcher.WatchDirectory("test_assets", false);

    // First update - detect existing file
    watcher.Update();
    assert(callbackCount == 1 && "Should detect file on first scan");

    // Delete the file
    DeleteTestFile("test_assets/test3.txt");

    // Second update - should detect deletion
    watcher.Update();
    assert(callbackCount == 2 && "Should detect file deletion");
    assert(lastAction == FileAction::Deleted && "Action should be Deleted");
    std::cout << "  ✓ File deletion detected successfully" << std::endl;

    // Cleanup
    fs::remove("test_assets");
}

// Test 4: Multiple Callbacks
void TestMultipleCallbacks() {
    std::cout << "\n[TEST 4] Multiple Callbacks:" << std::endl;

    FileWatcher watcher;
    int callback1Count = 0;
    int callback2Count = 0;

    // Create test directory
    fs::create_directory("test_assets");

    // Register multiple callbacks for same extension
    watcher.RegisterCallback(".txt", [&](const std::string&, FileAction) {
        callback1Count++;
    });

    watcher.RegisterCallback(".txt", [&](const std::string&, FileAction) {
        callback2Count++;
    });

    // Watch directory
    watcher.WatchDirectory("test_assets", false);

    // Create a file
    CreateTestFile("test_assets/test4.txt", "Content");
    watcher.Update();

    // Both callbacks should be invoked
    assert(callback1Count == 1 && "First callback should be invoked");
    assert(callback2Count == 1 && "Second callback should be invoked");
    std::cout << "  ✓ Multiple callbacks invoked successfully" << std::endl;

    // Cleanup
    DeleteTestFile("test_assets/test4.txt");
    fs::remove("test_assets");
}

// Test 5: Recursive Directory Watching
void TestRecursiveWatching() {
    std::cout << "\n[TEST 5] Recursive Directory Watching:" << std::endl;

    FileWatcher watcher;
    int callbackCount = 0;

    // Create test directory structure
    fs::create_directory("test_assets");
    fs::create_directory("test_assets/subdir");

    // Register callback
    watcher.RegisterCallback(".txt", [&](const std::string& path, FileAction) {
        callbackCount++;
        std::cout << "  Detected: " << path << std::endl;
    });

    // Watch directory recursively
    watcher.WatchDirectory("test_assets", true);

    // Create files in both root and subdirectory
    CreateTestFile("test_assets/root.txt", "Root content");
    CreateTestFile("test_assets/subdir/sub.txt", "Sub content");

    // Update should detect both files
    watcher.Update();
    assert(callbackCount == 2 && "Should detect files in subdirectories");
    std::cout << "  ✓ Recursive watching works correctly" << std::endl;

    // Cleanup
    DeleteTestFile("test_assets/root.txt");
    DeleteTestFile("test_assets/subdir/sub.txt");
    fs::remove("test_assets/subdir");
    fs::remove("test_assets");
}

// Test 6: Extension Filtering
void TestExtensionFiltering() {
    std::cout << "\n[TEST 6] Extension Filtering:" << std::endl;

    FileWatcher watcher;
    int txtCallbackCount = 0;
    int jsonCallbackCount = 0;

    // Create test directory
    fs::create_directory("test_assets");

    // Register callbacks for different extensions
    watcher.RegisterCallback(".txt", [&](const std::string&, FileAction) {
        txtCallbackCount++;
    });

    watcher.RegisterCallback(".json", [&](const std::string&, FileAction) {
        jsonCallbackCount++;
    });

    // Watch directory
    watcher.WatchDirectory("test_assets", false);

    // Create files with different extensions
    CreateTestFile("test_assets/file.txt", "Text");
    CreateTestFile("test_assets/data.json", "{}");
    CreateTestFile("test_assets/readme.md", "Markdown");

    // Update watcher
    watcher.Update();

    // Only registered extensions should trigger callbacks
    assert(txtCallbackCount == 1 && "TXT callback should be invoked once");
    assert(jsonCallbackCount == 1 && "JSON callback should be invoked once");
    std::cout << "  ✓ Extension filtering works correctly" << std::endl;

    // Cleanup
    DeleteTestFile("test_assets/file.txt");
    DeleteTestFile("test_assets/data.json");
    DeleteTestFile("test_assets/readme.md");
    fs::remove("test_assets");
}

// Test 7: Unwatch Directory
void TestUnwatchDirectory() {
    std::cout << "\n[TEST 7] Unwatch Directory:" << std::endl;

    FileWatcher watcher;
    int callbackCount = 0;

    // Create test directory
    fs::create_directory("test_assets");

    // Register callback
    watcher.RegisterCallback(".txt", [&](const std::string&, FileAction) {
        callbackCount++;
    });

    // Watch directory
    watcher.WatchDirectory("test_assets", false);

    // Create a file and update
    CreateTestFile("test_assets/test7.txt", "Content");
    watcher.Update();
    assert(callbackCount == 1 && "Should detect file while watching");

    // Unwatch directory
    watcher.UnwatchDirectory("test_assets");

    // Create another file and update
    CreateTestFile("test_assets/test8.txt", "More content");
    watcher.Update();
    assert(callbackCount == 1 && "Should not detect files after unwatching");
    std::cout << "  ✓ Unwatch directory works correctly" << std::endl;

    // Cleanup
    DeleteTestFile("test_assets/test7.txt");
    DeleteTestFile("test_assets/test8.txt");
    fs::remove("test_assets");
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
        std::cout << "All FileWatcher tests passed! ✓" << std::endl;
        std::cout << "===============================================" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n[ERROR] Test failed: " << e.what() << std::endl;
        return 1;
    }
}
