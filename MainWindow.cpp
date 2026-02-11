#pragma execution_character_set("utf-8")
#include "MainWindow.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    m_centralWidget = new QWidget(this);
    this->setCentralWidget(m_centralWidget);
    initLayout();

    QString modelPath = "vosk-model-small-cn-0.22"; // 将此路径替换为实际模型路径
    m_QSpeechRecognizer = new QSpeechRecognizer(modelPath);
    bool br = connect(m_QSpeechRecognizer, &QSpeechRecognizer::signalVoskData, this, &MainWindow::onVoskData);
    br = connect(m_QSpeechRecognizer, &QSpeechRecognizer::signalPrintLog, this, &MainWindow::onPrintLog);

    m_QSpeechRecognizer->startRecognition();
}

void MainWindow::onVoskData(const QString& data)
{
    onPrintLog("语音解析: " + data);
}

void MainWindow::initLayout()
{
    m_mainLayout = new QVBoxLayout(m_centralWidget);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);  // 去除边距
    m_mainLayout->setSpacing(1);                   // 左右间距（可调整）
    m_mainLayout->setStretch(0, 1);
    m_mainLayout->setStretch(1, 5);

    //QHBoxLayout* _btnLayout = new QHBoxLayout();
    //_btnLayout->setContentsMargins(0, 0, 0, 0);  // 去除边距
    //_btnLayout->setSpacing(0);                   // 左右间距（可调整）

    //QPushButton* _btn1 = new QPushButton("");
    //_btnLayout->addWidget(_btn1);

    //QPushButton* _btnStart = new QPushButton("开始采集");
    //QObject::connect(_btnStart, &QPushButton::clicked, this, [this]() {
    //        m_QSpeechRecognizer->SetEnable(true);
    //    });
    //_btnLayout->addWidget(_btnStart);
    //QPushButton* _btnFinish = new QPushButton("结束采集");
    //QObject::connect(_btnFinish, &QPushButton::clicked, this, [this]() {
    //    m_QSpeechRecognizer->SetEnable(false);
    //    });
    //_btnLayout->addWidget(_btnFinish);

    //QPushButton* _btn2 = new QPushButton("");
    //_btnLayout->addWidget(_btn2);

    //_btnLayout->setStretch(0, 3);
    //_btnLayout->setStretch(1, 1);
    //_btnLayout->setStretch(2, 1);
    //_btnLayout->setStretch(3, 3);
    //m_mainLayout->addLayout(_btnLayout);

    m_logTextEdit = new QPlainTextEdit(this);
    m_logTextEdit->setMinimumHeight(300);
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setFont(QFont("Consolas", 9));
    m_mainLayout->addWidget(m_logTextEdit);
}

void MainWindow::onPrintLog(const QString& logContent)
{
    // 线程安全：加锁避免多线程同时写入
    //QMutexLocker locker(&m_mutex);

    // 拼接日志：时间戳 + 日志级别 + 内容
    QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString log = QString("[%1] [%2] %3").arg(timeStr).arg("INFO").arg(logContent);

    // 追加日志到文本框（非阻塞，避免UI卡顿）
    QMetaObject::invokeMethod(m_logTextEdit, [this, log]() {
        // 追加文本（换行）
        m_logTextEdit->appendPlainText(log);
        // 自动滚动到底部（始终显示最新日志）
        QTextCursor cursor = m_logTextEdit->textCursor();
        cursor.movePosition(QTextCursor::End);
        m_logTextEdit->setTextCursor(cursor);
        }, Qt::QueuedConnection);
}

void MainWindow::onPrintNLog(QString logContent, int num)
{
    // 线程安全：加锁避免多线程同时写入
    //QMutexLocker locker(&m_mutex);
    logContent += QString::number(num);
    onPrintLog(logContent);
}
