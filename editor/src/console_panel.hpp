#pragma once

#include "editor_state.hpp"
#include <engine/core/log.hpp>
#include <QDockWidget>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QString>
#include <vector>
#include <mutex>

namespace editor {

// Log message entry
struct LogEntry {
    engine::core::LogLevel level;
    QString message;
    QString category;
    qint64 timestamp;
};

// Console panel showing log output and command input
class ConsolePanel : public QDockWidget {
    Q_OBJECT

public:
    explicit ConsolePanel(EditorState* state, QWidget* parent = nullptr);
    ~ConsolePanel() override;

    // Add a log message
    void log(engine::core::LogLevel level, const QString& message,
             const QString& category = "");

    // Clear all messages
    void clear();

    // Filter settings
    void set_filter_level(engine::core::LogLevel level);
    void set_filter_category(const QString& category);
    void set_search_text(const QString& text);

signals:
    void command_executed(const QString& command);
    void log_message_added(const LogEntry& entry);

private slots:
    void on_command_entered();
    void on_filter_changed();
    void on_clear_clicked();
    void on_log_added(const LogEntry& entry);

private:
    void setup_ui();
    void setup_connections();
    void apply_filters();
    void append_entry(const LogEntry& entry);
    QString format_entry(const LogEntry& entry) const;
    QColor color_for_level(engine::core::LogLevel level) const;

    EditorState* m_state;

    // UI components
    QPlainTextEdit* m_log_view;
    QLineEdit* m_command_input;
    QComboBox* m_level_filter;
    QLineEdit* m_search_filter;
    QCheckBox* m_auto_scroll;

    // Log storage
    std::vector<LogEntry> m_entries;
    std::mutex m_entries_mutex;

    // Filter settings
    engine::core::LogLevel m_min_level = engine::core::LogLevel::Trace;
    QString m_category_filter;
    QString m_search_text;
};

// Log sink that forwards messages to console panel
class ConsolePanelSink : public engine::core::ILogSink {
public:
    explicit ConsolePanelSink(ConsolePanel* console);

    void log(engine::core::LogLevel level, const std::string& category,
             const std::string& message) override;

private:
    ConsolePanel* m_console;
};

} // namespace editor
