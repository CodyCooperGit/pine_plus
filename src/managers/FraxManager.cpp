#include "FraxManager.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>

FraxManager::FraxManager(QObject* parent):
	ManagerBase(parent)
{
    m_inputKeyList << "type";
    m_inputKeyList << "country code";
    m_inputKeyList << "age";
    m_inputKeyList << "sex";
    m_inputKeyList << "bmi";
    m_inputKeyList << "previous fracture";
    m_inputKeyList << "parent hip fracture";
    m_inputKeyList << "current smoker";
    m_inputKeyList << "gluccocorticoid";
    m_inputKeyList << "rheumatoid arthritis";
    m_inputKeyList << "secondary osteoporosis";
    m_inputKeyList << "alcohol";
    m_inputKeyList << "femoral neck bmd";
}

void FraxManager::buildModel(QStandardItemModel *model) const
{
    // add the four probability measurements
    //
    for(int i = 0; i < m_test.getNumberOfMeasurements(); i++)
    {
        QStandardItem* item = model->item(i, 0);
        if (nullptr == item)
        {
            item = new QStandardItem();
            model->setItem(i, 0, item);
        }
        item->setData(m_test.getMeasurement(i).toString(), Qt::DisplayRole);
    }
}

void FraxManager::loadSettings(const QSettings& settings)
{
    // the full spec path name including exe name
    // eg., ../frax_module/blackbox.exe
    //
    QString exeName = settings.value("client/exe").toString();
    setExecutableName(exeName);
}

void FraxManager::saveSettings(QSettings* settings) const
{
    if (!m_executableName.isEmpty())
    {
        settings->setValue("client/exe", m_executableName);
        if (m_verbose)
            qDebug() << "wrote exe fullspec path to settings file";
    }
}

QJsonObject FraxManager::toJsonObject() const
{
    QJsonObject json = m_test.toJsonObject();
    if("simulate" != m_mode)
    {
      QFile ofile(m_outputFile);
      ofile.open(QIODevice::ReadOnly);
      QByteArray buffer = ofile.readAll();
      json.insert("test_output_file",QString(buffer.toBase64()));
      json.insert("test_output_file_mime_type","txt");
    }
    return json;
}

bool FraxManager::isDefined(const QString &exeName) const
{
    if("simulate" == m_mode)
    {
       return true;
    }
    bool ok = false;
    if(!exeName.isEmpty())
    {
        QFileInfo info(exeName);
        if(info.exists() && info.isExecutable())
        {
            ok = true;
        }
    }
    return ok;
}

void FraxManager::setExecutableName(const QString &exeName)
{
    if(isDefined(exeName))
    {
        QFileInfo info(exeName);
        m_executableName = exeName;
        m_executablePath = info.absolutePath();
        m_outputFile = QDir(m_executablePath).filePath("output.txt");
        m_inputFile =  QDir(m_executablePath).filePath("input.txt");
        m_temporaryFile = QDir(m_executablePath).filePath("input_ORIG.txt");

        if(QFileInfo::exists(m_inputFile))
          configureProcess();
        else
          qDebug() << "ERROR: expected default input.txt does not exist";
    }
}

void FraxManager::clean()
{
    if(!m_outputFile.isEmpty() && QFileInfo::exists(m_outputFile))
    {
        qDebug() << "removing output file " << m_outputFile;
        QFile ofile(m_outputFile);
        ofile.remove();
        m_outputFile.clear();
    }

    if(!m_temporaryFile.isEmpty() && QFileInfo::exists(m_temporaryFile))
    {
        // remove the inputfile first
        QFile ifile(m_inputFile);
        ifile.remove();
        QFile::copy(m_temporaryFile, m_inputFile);
        qDebug() << "restored backup from " << m_temporaryFile;
        QFile tempFile(m_temporaryFile);
        tempFile.remove();
        m_temporaryFile.clear();
    }
}

void FraxManager::measure()
{
    if("simulate" == m_mode)
    {
        readOutput();
        return;
    }
    // launch the process
    clearData();
    qDebug() << "starting process from measure";
    m_process.start();
}

void FraxManager::setInputData(const QMap<QString, QVariant> &input)
{
    if("simulate" == m_mode)
    {
        m_inputData["type"] = "t";
        m_inputData["country code"] = 19;
        m_inputData["age"] = 84.19;
        m_inputData["sex"] = 0;
        m_inputData["bmi"] = 24.07;
        m_inputData["previous fracture"] = 0;
        m_inputData["parent hip fracture"] = 0;
        m_inputData["current smoker"] = 0;
        m_inputData["gluccocorticoid"] = 0;
        m_inputData["rheumatoid arthritis"] = 0;
        m_inputData["secondary osteoporosis"] = 0;
        m_inputData["alcohol"] = 0;
        m_inputData["femoral_neck_bmd"] = -1.1;
        return;
    }
    bool ok = true;
    for(auto&& x : m_inputKeyList)
    {
        if(!input.contains(x))
        {
            ok = false;
            qDebug() << "ERROR: missing expected input " << x;
            break;
        }
        else
            m_inputData[x] = input[x];
    }
    if(!ok)
        m_inputData.clear();
    else
        configureProcess();
}

void FraxManager::readOutput()
{
    if("simulate" == m_mode)
    {
        qDebug() << "simulating read out";

        FraxMeasurement m;
        m.setCharacteristic("type","osteoporotic fracture");
        m.setCharacteristic("probability", 1.0);
        m.setCharacteristic("units","%");
        m_test.addMeasurement(m);
        m.setCharacteristic("type","hip fracture");
        m_test.addMeasurement(m);
        m.setCharacteristic("type","osteoporotic fracture bmd");
        m_test.addMeasurement(m);
        m.setCharacteristic("type","hip fracture bmd");
        m_test.addMeasurement(m);

        for(auto&& x : m_inputData.toStdMap())
        {
          m_test.addMetaDataCharacteristic(x.first,x.second);
        }
        emit canWrite();
        emit dataChanged();
        return;
    }

    if (QProcess::NormalExit != m_process.exitStatus())
    {
        qDebug() << "ERROR: process failed to finish correctly: cannot read output";
        return;
    }
    else
        qDebug() << "process finished successfully";

    if(QFileInfo::exists(m_outputFile))
    {
        qDebug() << "found output txt file " << m_outputFile;
        m_test.fromFile(m_outputFile);
        if(m_test.isValid())
        {
            emit canWrite();
        }
        else
            qDebug() << "ERROR: input from file produced invalid test results";

        emit dataChanged();
    }
    else
        qDebug() << "ERROR: no output.txt file found";
}

void FraxManager::configureProcess()
{
    if("simulate" == m_mode)
    {
        emit canMeasure();
        return;
    }

    // The exe and input file are present
    //
    QFileInfo info(m_executableName);
    QDir working(m_executablePath);
    if (info.exists() && info.isExecutable() &&
        working.exists() && QFileInfo::exists(m_inputFile))
    {
        qDebug() << "OK: configuring command";

        m_process.setProcessChannelMode(QProcess::ForwardedChannels);
        m_process.setProgram(m_executableName);
        m_process.setWorkingDirectory(m_executablePath);

        qDebug() << "process working dir: " << m_executablePath;

        // backup the original intput.txt
        if(QFileInfo::exists(m_temporaryFile))
        {
            QFile tfile(m_temporaryFile);
            tfile.remove();
        }
        QFile::copy(m_inputFile, m_temporaryFile);
        qDebug() << "wrote backup to " << m_temporaryFile;

        // generate the input.txt file content
        if(m_inputData.isEmpty())
        {
            qDebug() << "ERROR: no input data to write to input.txt";
            return;
        }
        QStringList list;
        for(auto&& x : m_inputData.values())
        {
            list << x.toString();
        }
        QString line = list.join(",");
        QFile ofile(m_inputFile);
        if(ofile.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextStream stream(&ofile);
            stream << line;
            ofile.close();
            qDebug() << "populated the input.txt file " << m_inputFile;
            qDebug() << "content should be " << line;
        }
        else
        {
            qDebug() << "ERROR: failed writing to " << m_inputFile;
            return;
        }

        connect(&m_process, &QProcess::started,
            this, [this]() {
                qDebug() << "process started: " << m_process.arguments().join(" ");
            });

        connect(&m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &FraxManager::readOutput);

        connect(&m_process, &QProcess::errorOccurred,
            this, [](QProcess::ProcessError error)
            {
                QStringList s = QVariant::fromValue(error).toString().split(QRegExp("(?=[A-Z])"), QString::SkipEmptyParts);
                qDebug() << "ERROR: process error occured: " << s.join(" ").toLower();
            });

        connect(&m_process, &QProcess::stateChanged,
            this, [](QProcess::ProcessState state) {
                QStringList s = QVariant::fromValue(state).toString().split(QRegExp("(?=[A-Z])"), QString::SkipEmptyParts);
                qDebug() << "process state: " << s.join(" ").toLower();

            });

        emit canMeasure();
    }
    else
        qDebug() << "failed to configure process";
}

void FraxManager::clearData()
{
    m_test.reset();
    emit dataChanged();
}