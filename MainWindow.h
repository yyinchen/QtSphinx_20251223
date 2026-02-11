#pragma once
#include <QtWidgets/qmainwindow.h>
#include <QtWidgets\QMainWindow>
#include <QtWidgets\QVBoxLayout> 
#include <QtWidgets\QHBoxLayout>
#include <QtWidgets\QLabel>
#include <QtWidgets\QPushButton>
#include <QtWidgets\QProgressBar>
#include <QtCharts/QChartView>
#include <QtWidgets\QComboBox>
#include <QtWidgets\QLineEdit>
#include <QtWidgets\QPlainTextEdit>
#include <QTimer>  // 定时器头文件
#include <QDateTime> // 时间类头文件

#include "QSpeechRecognizer.hpp"

class MainWindow :public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow() {};

public slots:
    void onPrintLog(const QString& logContent);
    void onVoskData(const QString& data);
    void onPrintNLog(QString logContent, int num);

private:
    // 初始化整体左右布局
    void initLayout();
    

    QSpeechRecognizer* m_QSpeechRecognizer = nullptr;
    QWidget* m_centralWidget = nullptr;  // 中心部件
    QVBoxLayout* m_mainLayout = nullptr; // 主布局
    QPlainTextEdit* m_logTextEdit = nullptr;
};

