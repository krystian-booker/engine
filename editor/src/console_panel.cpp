#include "console_panel.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QPushButton>
#include <QLabel>
#include <QDateTime>
#include <QTextCursor>
#include <QScrollBar>

namespace editor {

ConsolePanel::ConsolePanel(EditorState* state, QWidget* parent)
    : QDockWidget("Console", parent)
    , m_state(state)
{
    setup_ui();
    setup_connections();
}

ConsolePanel::~ConsolePanel() = default;

void ConsolePanel::setup_ui() {
    auto* container = new QWidget(this);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    // Toolbar
    auto* toolbar = new QWidget(container);
    auto* toolbar_layout = new QHBoxLayout(toolbar);
    toolbar_layout->setContentsMargins(4, 2, 4, 2);
    toolbar_layout->setSpacing(4);

    auto* clear_btn = new QPushButton("Clear", toolbar);
    connect(clear_btn, &QPushButton::clicked, this, &ConsolePanel::on_clear_clicked);
    toolbar_layout->addWidget(clear_btn);

    toolbar_layout->addWidget(new QLabel("Level:", toolbar));
    m_level_filter = new QComboBox(toolbar);
    m_level_filter->addItem("Trace", static_cast<int>(engine::core::LogLevel::Trace));
    m_level_filter->addItem("Debug", static_cast<int>(engine::core::LogLevel::Debug));
    m_level_filter->addItem("Info", static_cast<int>(engine::core::LogLevel::Info));
    m_level_filter->addItem("Warn", static_cast<int>(engine::core::LogLevel::Warn));
    m_level_filter->addItem("Error", static_cast<int>(engine::core::LogLevel::Error));
    m_level_filter->setCurrentIndex(2); // Info
    toolbar_layout->addWidget(m_level_filter);

    toolbar_layout->addWidget(new QLabel("Search:", toolbar));
    m_search_filter = new QLineEdit(toolbar);
    m_search_filter->setPlaceholderText("Filter...");
    m_search_filter->setClearButtonEnabled(true);
    toolbar_layout->addWidget(m_search_filter, 1);

    m_auto_scroll = new QCheckBox("Auto-scroll", toolbar);
    m_auto_scroll->setChecked(true);
    toolbar_layout->addWidget(m_auto_scroll);

    layout->addWidget(toolbar);

    // Log view
    m_log_view = new QPlainTextEdit(container);
    m_log_view->setReadOnly(true);
    m_log_view->setMaximumBlockCount(10000);
    m_log_view->setFont(QFont("Consolas", 9));
    m_log_view->setLineWrapMode(QPlainTextEdit::NoWrap);
    layout->addWidget(m_log_view, 1);

    // Command input
    auto* input_container = new QWidget(container);
    auto* input_layout = new QHBoxLayout(input_container);
    input_layout->setContentsMargins(4, 2, 4, 2);

    auto* prompt = new QLabel(">", input_container);
    input_layout->addWidget(prompt);

    m_command_input = new QLineEdit(input_container);
    m_command_input->setPlaceholderText("Enter command...");
    input_layout->addWidget(m_command_input, 1);

    layout->addWidget(input_container);

    setWidget(container);
}

void ConsolePanel::setup_connections() {
    connect(m_command_input, &QLineEdit::returnPressed,
            this, &ConsolePanel::on_command_entered);

    connect(m_level_filter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConsolePanel::on_filter_changed);

    connect(m_search_filter, &QLineEdit::textChanged,
            this, &ConsolePanel::on_filter_changed);

    connect(this, &ConsolePanel::log_message_added,
            this, &ConsolePanel::on_log_added, Qt::QueuedConnection);
}

void ConsolePanel::log(engine::core::LogLevel level, const QString& message,
                        const QString& category) {
    LogEntry entry;
    entry.level = level;
    entry.message = message;
    entry.category = category;
    entry.timestamp = QDateTime::currentMSecsSinceEpoch();

    {
        std::lock_guard<std::mutex> lock(m_entries_mutex);
        m_entries.push_back(entry);
    }

    emit log_message_added(entry);
}

void ConsolePanel::clear() {
    {
        std::lock_guard<std::mutex> lock(m_entries_mutex);
        m_entries.clear();
    }
    m_log_view->clear();
}

void ConsolePanel::set_filter_level(engine::core::LogLevel level) {
    m_min_level = level;
    int index = m_level_filter->findData(static_cast<int>(level));
    if (index >= 0) {
        m_level_filter->setCurrentIndex(index);
    }
    apply_filters();
}

void ConsolePanel::set_filter_category(const QString& category) {
    m_category_filter = category;
    apply_filters();
}

void ConsolePanel::set_search_text(const QString& text) {
    m_search_filter->setText(text);
    apply_filters();
}

void ConsolePanel::on_command_entered() {
    QString command = m_command_input->text().trimmed();
    if (command.isEmpty()) return;

    // Echo command
    log(engine::core::LogLevel::Info, "> " + command, "Console");

    // TODO: Implement command processing
    if (command == "clear") {
        clear();
    } else if (command == "help") {
        log(engine::core::LogLevel::Info, "Available commands:", "Console");
        log(engine::core::LogLevel::Info, "  clear - Clear console", "Console");
        log(engine::core::LogLevel::Info, "  help  - Show this help", "Console");
    } else {
        log(engine::core::LogLevel::Warn, "Unknown command: " + command, "Console");
    }

    m_command_input->clear();
    emit command_executed(command);
}

void ConsolePanel::on_filter_changed() {
    m_min_level = static_cast<engine::core::LogLevel>(
        m_level_filter->currentData().toInt());
    m_search_text = m_search_filter->text();
    apply_filters();
}

void ConsolePanel::on_clear_clicked() {
    clear();
}

void ConsolePanel::on_log_added(const LogEntry& entry) {
    // Check filters
    if (entry.level < m_min_level) return;

    if (!m_search_text.isEmpty() &&
        !entry.message.contains(m_search_text, Qt::CaseInsensitive) &&
        !entry.category.contains(m_search_text, Qt::CaseInsensitive)) {
        return;
    }

    append_entry(entry);
}

void ConsolePanel::apply_filters() {
    m_log_view->clear();

    std::lock_guard<std::mutex> lock(m_entries_mutex);
    for (const auto& entry : m_entries) {
        if (entry.level < m_min_level) continue;

        if (!m_search_text.isEmpty() &&
            !entry.message.contains(m_search_text, Qt::CaseInsensitive) &&
            !entry.category.contains(m_search_text, Qt::CaseInsensitive)) {
            continue;
        }

        append_entry(entry);
    }
}

void ConsolePanel::append_entry(const LogEntry& entry) {
    QString formatted = format_entry(entry);
    QColor color = color_for_level(entry.level);

    QTextCharFormat format;
    format.setForeground(color);

    QTextCursor cursor = m_log_view->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(formatted + "\n", format);

    if (m_auto_scroll->isChecked()) {
        m_log_view->verticalScrollBar()->setValue(
            m_log_view->verticalScrollBar()->maximum());
    }
}

QString ConsolePanel::format_entry(const LogEntry& entry) const {
    QDateTime time = QDateTime::fromMSecsSinceEpoch(entry.timestamp);
    QString time_str = time.toString("hh:mm:ss.zzz");

    QString level_str;
    switch (entry.level) {
        case engine::core::LogLevel::Trace: level_str = "TRC"; break;
        case engine::core::LogLevel::Debug: level_str = "DBG"; break;
        case engine::core::LogLevel::Info:  level_str = "INF"; break;
        case engine::core::LogLevel::Warn:  level_str = "WRN"; break;
        case engine::core::LogLevel::Error: level_str = "ERR"; break;
        case engine::core::LogLevel::Fatal: level_str = "FTL"; break;
    }

    if (entry.category.isEmpty()) {
        return QString("[%1] [%2] %3").arg(time_str, level_str, entry.message);
    } else {
        return QString("[%1] [%2] [%3] %4")
            .arg(time_str, level_str, entry.category, entry.message);
    }
}

QColor ConsolePanel::color_for_level(engine::core::LogLevel level) const {
    switch (level) {
        case engine::core::LogLevel::Trace: return QColor(128, 128, 128);
        case engine::core::LogLevel::Debug: return QColor(180, 180, 180);
        case engine::core::LogLevel::Info:  return QColor(255, 255, 255);
        case engine::core::LogLevel::Warn:  return QColor(255, 200, 100);
        case engine::core::LogLevel::Error: return QColor(255, 100, 100);
        case engine::core::LogLevel::Fatal: return QColor(255, 50, 50);
        default: return QColor(255, 255, 255);
    }
}

} // namespace editor
