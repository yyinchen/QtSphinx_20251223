#pragma once
//#pragma execution_character_set("utf-8")
#include <QObject>
#include <QtMultimedia/QAudioInput>
#include <QtMultimedia/qaudiodeviceinfo.h>
#include <QtGui\QFont>
#include <QBuffer>
#include <QFile>
#include <Qdebug>
#include <QTimer>
#include <QDataStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <cstdint>
#include <mutex>
#include <vosk-win64-0.3.45/vosk_api.h>
#include <iostream>
// Windows平台需额外包含
#ifdef Q_OS_WIN
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

class QSpeechRecognizer : public QObject
{
    Q_OBJECT
public:
    QSpeechRecognizer(const QString& modelPath, QObject* parent = nullptr)
        : QObject(parent) 
    {
        m_model = vosk_model_new(modelPath.toStdString().c_str());
        if (!m_model)
        {
            emit signalPrintLog("模型加载失败！");
            return;
        }
        const char* keywords =
            R"([ "打开", "关闭",  "曝光", "语音", "运动", "底盘",  "控制",
                "前进", "后退", "左", "向", "向右", "移动","停",    
                "下", "一张", "上", "正位", "侧", "位","图像", "镜像", "翻转" ,    
                "复辟", "旋转", "水平", "上下", "收起"])";
        m_recognizer = vosk_recognizer_new_grm(m_model, 16000.0, keywords);
        // 可选：设置识别阈值（减少误识别，0~1，越小越严格）
        vosk_recognizer_set_max_alternatives(m_recognizer, 0);
        vosk_recognizer_set_words(m_recognizer, 1);
        vosk_recognizer_set_partial_words(m_recognizer, 0);
    }

    ~QSpeechRecognizer() 
    {
        vosk_recognizer_free(m_recognizer);
        vosk_model_free(m_model);
    }

    //void SetEnable(bool b)
    //{
    //    m_bEnable = b;
    // }

    void startRecognition() 
    {
#ifdef Q_OS_WIN
        qputenv("QT_AUDIO_BACKEND", "wasapi");
#endif
        QAudioFormat format;
        format.setSampleRate(16000);
        format.setChannelCount(1);
        format.setSampleSize(16);
        format.setCodec("audio/pcm");//定义音频为原始 PCM 编码
        format.setByteOrder(QAudioFormat::LittleEndian);//定义多字节采样值的存储顺序,	台式机选小端序，嵌入式按需选大端序
        format.setSampleType(QAudioFormat::SignedInt);//定义采样值的数值类型	16 位采样选有符号整型，8 位选无符号

        QAudioDeviceInfo inputDev;
        QList<QAudioDeviceInfo> availableInputs = removeDuplicateAudioDevices(QAudioDeviceInfo::availableDevices(QAudio::AudioInput));
        emit signalPrintLog("枚举到的输入设备数："+ QString::number(availableInputs.size()));
        if (availableInputs.isEmpty()) 
            inputDev = QAudioDeviceInfo::defaultInputDevice();
        else
            inputDev = availableInputs[0];

        if (!inputDev.isFormatSupported(format)) 
            format = inputDev.nearestFormat(format);

        qDebug() << "设备名：" << inputDev.deviceName();
        qDebug() << "支持的采样率：" << inputDev.supportedSampleRates();
        qDebug() << "支持的声道数：" << inputDev.supportedChannelCounts();
        qDebug() << "支持的采样位深：" << inputDev.supportedSampleSizes();
        emit signalPrintLog("设备名："+ inputDev.deviceName());

        QList<int> numList = inputDev.supportedSampleRates();
        QString result2;
        for (int i = 0; i < numList.size(); ++i)
        {
            result2 += QString::number(numList[i]);
            if (i != numList.size() - 1) 
                result2 += " | "; // 自定义分隔符（竖线）
        }
        emit signalPrintLog("支持的采样率：" + result2);
        
        result2 = " ";
        numList = inputDev.supportedChannelCounts();
        for (int i = 0; i < numList.size(); ++i)
        {
            result2 += QString::number(numList[i]);
            if (i != numList.size() - 1)
                result2 += " | "; // 自定义分隔符（竖线）
        }
        emit signalPrintLog("支持的声道数：" + result2);

        result2 = " ";
        numList = inputDev.supportedSampleSizes();
        for (int i = 0; i < numList.size(); ++i)
        {
            result2 += QString::number(numList[i]);
            if (i != numList.size() - 1)
                result2 += " | "; // 自定义分隔符（竖线）
        }
        emit signalPrintLog("支持的采样位深：" + result2);

        if (m_audioInput) delete m_audioInput;
        m_audioInput = nullptr;

        m_audioInput = new QAudioInput(inputDev, format, this);
        //m_audioInput->setVolume(0.1);
        m_audioInput->setNotifyInterval(500);

        connect(m_audioInput, &QAudioInput::notify, this, &QSpeechRecognizer::onNotify);
        connect(m_audioInput, &QAudioInput::stateChanged, this, &QSpeechRecognizer::onStateChanged);
        m_audioBuffer.open(QIODevice::ReadWrite); //(QIODevice::WriteOnly | QIODevice::Truncate);
        m_audioInput->start(&m_audioBuffer);

        //QString wavPath = "D:/Code/QtSphinx_20251223/MP3/CloseProgram.wav"; // 你的WAV文件路径（需符合格式要求）
        //m_pcmData = readWavPcmData(wavPath, format);
        //if (m_pcmData.isEmpty()) 
        //    return  ;
        //vosk_recognizer_accept_waveform(recognizer, m_pcmData.constData(), m_pcmData.size());
        //const char* result = vosk_recognizer_final_result(recognizer);
        //qDebug() << "识别结果：" << result;
    }

    // 解析WAV文件，提取PCM原始数据（返回空则解析失败）
    QByteArray readWavPcmData(const QString& wavFilePath, QAudioFormat& targetFormat)
    {
        QFile wavFile(wavFilePath);
        if (!wavFile.open(QIODevice::ReadOnly)) {
            qDebug() << "[错误] 打开文件失败：" << wavFile.errorString();
            return QByteArray();
        }

        // ========== 用QDataStream读取（避免字节对齐问题） ==========
        QDataStream in(&wavFile);
        in.setByteOrder(QDataStream::LittleEndian); // WAV是小端序
        in.setFloatingPointPrecision(QDataStream::SinglePrecision);

        // 1. 验证RIFF标识
        char riff[4];
        in.readRawData(riff, 4);
        if (memcmp(riff, "RIFF", 4) != 0) {
            qDebug() << "[错误] RIFF标识缺失，文件头：" << QByteArray(riff, 4).toHex();
            wavFile.close();
            return QByteArray();
        }

        // 2. 跳过文件大小 + 验证WAVE标识
        quint32 fileSize;
        in >> fileSize; // 读取文件大小（4字节）
        char wave[4];
        in.readRawData(wave, 4);
        if (memcmp(wave, "WAVE", 4) != 0) {
            qDebug() << "[错误] WAVE标识缺失，文件头：" << QByteArray(wave, 4).toHex();
            wavFile.close();
            return QByteArray();
        }

        // 3. 遍历块找data块
        char chunkId[4];
        quint32 chunkSize;
        QByteArray pcmData;
        while (!in.atEnd()) {
            in.readRawData(chunkId, 4); // 块ID
            if (in.status() != QDataStream::Ok) break;

            in >> chunkSize; // 块大小（4字节小端序）
            if (in.status() != QDataStream::Ok) break;

            // 找到data块
            if (memcmp(chunkId, "data", 4) == 0) {
                char* dataBuf = new char[chunkSize];
                in.readRawData(dataBuf, chunkSize);
                pcmData = QByteArray(dataBuf, chunkSize);
                delete[] dataBuf;
                qDebug() << "[成功] 读取PCM数据：" << chunkSize << "字节";
                break;
            }

            // 跳过非data块（对齐偶数字节）
            quint32 skip = chunkSize + (chunkSize % 2);
            in.skipRawData(skip);
        }

        wavFile.close();

        // 验证PCM数据
        if (pcmData.isEmpty()) {
            qDebug() << "[错误] 未找到data块或数据为空";
            return QByteArray();
        }
        bool isAllZero = std::all_of(pcmData.begin(), pcmData.end(), [](char c) { return c == 0; });
        if (isAllZero) {
            qDebug() << "[警告] PCM数据全为0，音频为空";
        }

        return pcmData;
    }

    QList<QAudioDeviceInfo> removeDuplicateAudioDevices(const QList<QAudioDeviceInfo>& deviceList)
    {
        QList<QAudioDeviceInfo> uniqueList;
        QSet<QString> deviceNames; // 存储已出现的设备名称，快速判重

        for (const QAudioDeviceInfo& dev : deviceList) {
            QString devName = dev.deviceName().trimmed(); // 去除首尾空格，避免因空格导致的“伪重复”
            if (devName.isEmpty()) {
                continue; // 跳过空名称的无效设备
            }

            // 若设备名称未出现过，加入结果列表，并记录名称
            if (!deviceNames.contains(devName)) {
                uniqueList.append(dev);
                deviceNames.insert(devName);
            }
        }

        for (auto& dev : uniqueList)
            emit signalPrintLog("设备名称：" + dev.deviceName());

        return uniqueList;
    }

private slots:
    void onStateChanged(QAudio::State state) 
    {
        int _state = -1;
        if (state == QAudio::IdleState)
        {
            //空闲状态   
            // 无新的音频数据可读取（如麦克风静音 / 无声音）；
            //播放队列空，无数据可播放
            //m_audioInput->stop();
            //m_audioBuffer.close();
            //processAudio();
            _state = 3;
        }
        else if (state == QAudio::ActiveState )
        {
            _state = 0;//音频设备正在主动采集 / 播放数据
        }
        else if (state == QAudio::SuspendedState)
        {
            _state = 1;//调用 suspend() 主动暂停音频操作
        }
        else if (state == QAudio::StoppedState)
        {
            //停止状态
            /* 1. 调用 stop() 主动停止；
                2. 音频格式不兼容；
                3. 设备打开失败（权限 / 服务问题）；
                4. 数据读写错误；
                5. 设备被移除*/
            _state = 2;
        }
        else if (state == QAudio::InterruptedState)
        {
            //中断状态
            //音频设备被系统 / 其他应用抢占（Qt 6+ 新增）
            _state = 4;
        }
        emit signalPrintLog("onStateChanged: state  " + QString::number(_state));
    }

    void processAudio() 
    {
        m_audioBuffer.seek(0);
        QByteArray audioData = m_audioBuffer.readAll();// buffer();
        int length = audioData.size();
        const char* data = audioData.data();

        if (vosk_recognizer_accept_waveform(m_recognizer, data, length))
        {
            const char* ch = vosk_recognizer_result(m_recognizer);
            m_audioBuffer.buffer().clear();
            m_audioBuffer.seek(0);

            //emit signalPrintLog(QString::fromUtf8(ch));
            QString qStr1 = parseSpeechResult(QString::fromUtf8(ch));
            if (qStr1.contains("打开语音"))
            {
                emit signalVoskData(qStr1);
                m_bEnable = true;
            }
            else if (qStr1.contains("关闭语音"))
            {
                emit signalVoskData(qStr1);
                m_bEnable = false;
            }
            else if ("" != qStr1 && m_bEnable)
                emit signalVoskData(qStr1);
        }
        //else 
        //{
        //    const char* ch = vosk_recognizer_partial_result(m_recognizer);
        //    //std::cout << ch << std::endl;
        //    //m_audioBuffer.buffer().clear();
        //    //m_audioBuffer.seek(0);
        //    //emit signalPrintLog(QString::fromUtf8(ch));

        //    QString qStr1 = parseSpeechResult(QString::fromUtf8(ch));
        //    if ("" != qStr1)
        //        emit signalVoskData(qStr1);
        //}
    }

    void processAudio(QByteArray audioData) 
    {
        int length = audioData.size();
        const char* data = audioData.data();

        if (vosk_recognizer_accept_waveform(m_recognizer, data, length)) {
            std::cout << vosk_recognizer_result(m_recognizer) << std::endl;
        }
        else {
            std::cout << vosk_recognizer_partial_result(m_recognizer) << std::endl;
        }
    }

    void onNotify()
    {
        if (m_mutex.try_lock())
        {
            processAudio();
            m_mutex.unlock();
        }
           
        //m_audioBuffer.seek(0);
        //QByteArray ba = m_audioBuffer.readAll();
        //m_audioBuffer.buffer().clear();
        //m_audioBuffer.seek(0);
    }

signals:
    void signalVoskData(const QString& data);
    void signalPrintLog(const QString& logContent);

private:
    QString parseSpeechResult(const QString& jsonStr)
    {
        // 1. 将JSON字符串转为QJsonDocument
        QJsonParseError parseError;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonStr.toUtf8(), &parseError);

        // 异常处理：解析失败
        if (parseError.error != QJsonParseError::NoError) {
            qCritical() << "JSON解析失败：" << parseError.errorString();
            return "";
        }

        // 2. 解析根对象（外层是{}，所以是QJsonObject）
        if (!jsonDoc.isObject()) {
            qCritical() << "JSON根节点不是对象！";
            return "";
        }
        QJsonObject rootObj = jsonDoc.object();

        QString fullText = "";
        if (rootObj.contains("text")  && ""  != rootObj["text"].toString())
        {
            //fullText += "\n 完整文本：";
            QStringList  res = rootObj["text"].toString().split(" ");
            for (auto s : res)
            {
                if(!fullText.contains(s))
                    fullText += s;
            }
            fullText += " \n ";
        }

        //if (rootObj.contains("result") && rootObj["result"].isArray())
        //{
        //    QJsonArray resultArray = rootObj["result"].toArray();

        //    if(resultArray.size()>0)
        //        fullText += "\n     识别结果详情：result";
        //    for (int i = 0; i < resultArray.size(); ++i) 
        //    {
        //        QJsonObject wordObj = resultArray[i].toObject();

        //        double conf = wordObj["conf"].toDouble(0.0);       // 置信度
        //        double start = wordObj["start"].toDouble(0.0);     // 开始时间
        //        double end = wordObj["end"].toDouble(0.0);         // 结束时间
        //        QString word = wordObj["word"].toString("");       // 识别的词

        //        fullText += QString("\n     第%1个词：%2 | 置信度：%3 | 时间区间：%4 - %5秒")
        //            .arg(i + 1)
        //            .arg(word)
        //            .arg(conf, 0, 'f', 3)  // 保留3位小数
        //            .arg(start, 0, 'f', 3)
        //            .arg(end, 0, 'f', 3);
        //    }
        //}

        //if (rootObj.contains("partial") && rootObj["partial"].isArray())
        //{
        //    QJsonArray resultArray = rootObj["partial"].toArray();
        //    if (resultArray.size() > 0)
        //        fullText += "\n     识别结果详情：partial";

        //    for (int i = 0; i < resultArray.size(); ++i)
        //    {
        //        QJsonObject wordObj = resultArray[i].toObject();

        //        double conf = wordObj["conf"].toDouble(0.0);       // 置信度
        //        double start = wordObj["start"].toDouble(0.0);     // 开始时间
        //        double end = wordObj["end"].toDouble(0.0);         // 结束时间
        //        QString word = wordObj["word"].toString("");       // 识别的词

        //        fullText += QString("\n     第%1个词：%2 | 置信度：%3 | 时间区间：%4 - %5秒")
        //            .arg(i + 1)
        //            .arg(word)
        //            .arg(conf, 0, 'f', 3)  // 保留3位小数
        //            .arg(start, 0, 'f', 3)
        //            .arg(end, 0, 'f', 3);
        //    }
        //}

        return fullText;
    }

    VoskModel* m_model;
    VoskRecognizer* m_recognizer;
    QAudioInput* m_audioInput;
    QBuffer m_audioBuffer;
    QByteArray m_pcmData;
    int m_currentPos = 0;
    bool m_bEnable = false;

    QTimer* m_dataCheckTimer = nullptr;

    // 数据检测变量
    qint64 m_lastBufferSize = 0;    // 上次缓冲区大小
    bool m_isSpeaking = false;      // 是否正在说话
    bool m_isDeviceActive = false;  // 设备是否处于ActiveState

    std::mutex m_mutex;
};
