#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QCloseEvent>
#include <QDate>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMessageBox>
#include <QSettings>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_verbose(false)
    , m_manager(this)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initialize()
{
  m_manager.setVerbose(m_verbose);

  readInput();

  // Connect the serial port property notify signal to catch
  // read from ini file
  //
  connect(&m_manager, &WeighScaleManager::portNameChanged,
          ui->serialPortLineEdit, &QLineEdit::setText);

  // Read the .ini file for cached local and peripheral device addresses
  //
  QDir dir = QCoreApplication::applicationDirPath();
  QSettings settings(dir.filePath("weighscale.ini"), QSettings::IniFormat);
  m_manager.loadSettings(settings);

  // Save button to store measurement and device info to .json
  //
  ui->saveButton->setEnabled(false);

  // Zero the scale
  //
  ui->zeroButton->setEnabled(false);

  // Read the weight measurement off the scale
  //
  ui->measureButton->setEnabled(false);

  // as soon as there are serial ports in the list, allow click to select port
  //
  connect(ui->serialPortListWidget, &QListWidget::itemDoubleClicked,
          this,[this](QListWidgetItem* item)
    {
      if(m_verbose)
          qDebug() << "device selected from list " <<  item->text();
      m_manager.selectDevice(item->text());
    }
  );

  connect(&m_manager, &WeighScaleManager::scanning,
          this,[this]()
    {
      ui->serialPortListWidget->clear();
      ui->statusBar->showMessage("Discovering serial ports...");
    }
  );

  connect(&m_manager, &WeighScaleManager::discovered,
          this, &MainWindow::updateDeviceList);

  connect(&m_manager, &WeighScaleManager::measured,
          this, &MainWindow::updateMeasurementList);

  connect(&m_manager, &WeighScaleManager::canSelect,
          this,[this](){
      ui->statusBar->showMessage("Ready to select...");
      // Prompt the user to select the MAC address
      QMessageBox msgBox;
      msgBox.setText(tr("Double click the port from the list.  If the device "
        "is not in the list, quit the application and check that the port is "
        "working and connect the weigh scale to it before running this application."));
      msgBox.setIcon(QMessageBox::Warning);
      msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Abort);
      msgBox.setButtonText(QMessageBox::Abort,tr("Quit"));
      connect(msgBox.button(QMessageBox::Abort),&QPushButton::clicked,this,&MainWindow::close);
      msgBox.exec();
  });

  connect(ui->zeroButton, &QPushButton::clicked,
        &m_manager, &WeighScaleManager::zero);

  connect(&m_manager, &WeighScaleManager::canConnect,
          this,[this](){
      qDebug() << "ready to zero";
      ui->statusBar->showMessage("Ready to zero...");
      ui->zeroButton->setEnabled(true);
      ui->measureButton->setEnabled(false);
      ui->saveButton->setEnabled(false);
  });

  connect(ui->measureButton, &QPushButton::clicked,
        &m_manager, &WeighScaleManager::measure);

  connect(&m_manager, &WeighScaleManager::canMeasure,
          this,[this](){
      qDebug() << "ready to measure";
      ui->statusBar->showMessage("Ready to measure...");
      ui->measureButton->setEnabled(true);
  });

  connect(&m_manager, &WeighScaleManager::canWrite,
          this,[this](){
      qDebug() << "ready to write";
      ui->statusBar->showMessage("Ready to write...");
      ui->saveButton->setEnabled(true);
  });


  if(m_inputData.contains("Barcode") && m_inputData["Barcode"].isValid())
     ui->barcodeLineEdit->setText(m_inputData["Barcode"].toString());
  else
     ui->barcodeLineEdit->setText("00000000"); // dummy

  connect(ui->saveButton, &QPushButton::clicked,
    this, [this]{
      writeOutput();
    }
  );
}

void MainWindow::updateDeviceList(const QString &label)
{
    // Add the device to the list
    //
    QList<QListWidgetItem *> items = ui->serialPortListWidget->findItems(label, Qt::MatchExactly);
    if(items.empty())
    {
        QListWidgetItem *item = new QListWidgetItem(label);

        // TODO: consider checking if the weigh scale is running on
        // the port
        if(m_manager.isDefined(label))
          item->setForeground(QColor(Qt::green));
        else
          item->setForeground(QColor(Qt::black));

        ui->serialPortListWidget->addItem(item);
    }
}

void MainWindow::updateMeasurementList(const QString &label)
{
    // Add the device to the list
    //
    QList<QListWidgetItem *> items = ui->measurementListWidget->findItems(label, Qt::MatchExactly);
    if(items.empty())
    {
        QListWidgetItem *item = new QListWidgetItem(label);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
        ui->measurementListWidget->addItem(item);
    }
}

void MainWindow::run()
{
    m_manager.scanDevices();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if(m_verbose)
        qDebug() << "close event called";
    QDir dir = QCoreApplication::applicationDirPath();
    QSettings settings(dir.filePath("weighscale.ini"), QSettings::IniFormat);
    m_manager.saveSettings(&settings);

    event->accept();
}

void MainWindow::readInput()
{
    // TODO: if the run mode is not debug, an input file name is mandatory, throw an error
    //
    if(m_inputFileName.isEmpty())
    {
        qDebug() << "no input file";
        return;
    }
    QFileInfo info(m_inputFileName);
    if(info.exists())
    {
      QFile file;
      file.setFileName(m_inputFileName);
      file.open(QIODevice::ReadOnly | QIODevice::Text);
      QString val = file.readAll();
      file.close();
      qDebug() << val;

      QJsonDocument jsonDoc = QJsonDocument::fromJson(val.toUtf8());
      QJsonObject jsonObj = jsonDoc.object();
      QMapIterator<QString,QVariant> it(m_inputData);
      QList<QString> keys = jsonObj.keys();
      for(int i=0;i<keys.size();i++)
      {
          QJsonValue v = jsonObj.value(keys[i]);
          // TODO: error report all missing expected key values
          //
          if(!v.isUndefined())
          {
              m_inputData[keys[i]] = v.toVariant();
              qDebug() << keys[i] << v.toVariant();
          }
      }
    }
    else
        qDebug() << m_inputFileName << " file does not exist";
}

void MainWindow::writeOutput()
{
   if(m_verbose)
       qDebug() << "begin write process ... ";

   QJsonObject jsonObj = m_manager.toJsonObject();

   qDebug() << "received json measurement data";

   QString barcode = ui->barcodeLineEdit->text().simplified().remove(" ");
   jsonObj.insert("barcode",QJsonValue(barcode));

   if(m_verbose)
       qDebug() << "determine file output name ... ";

   QString fileName;

   // Use the output filename if it has a valid path
   // If the path is invalid, use the directory where the application exe resides
   // If the output filename is empty default output .json file is of the form
   // <participant ID>_<now>_<devicename>.json
   //
   bool constructDefault = false;

   // TODO: if the run mode is not debug, an output file name is mandatory, throw an error
   //
   if(m_outputFileName.isEmpty())
       constructDefault = true;
   else
   {
     QFileInfo info(m_outputFileName);
     QDir dir = info.absoluteDir();
     if(dir.exists())
       fileName = m_outputFileName;
     else
       constructDefault = true;
   }
   if(constructDefault)
   {
       QDir dir = QCoreApplication::applicationDirPath();
       if(m_outputFileName.isEmpty())
       {
         QStringList list;
         list << barcode;
         list << QDate().currentDate().toString("yyyyMMdd");
         list << "weighscale.json";
         fileName = dir.filePath( list.join("_") );
       }
       else
         fileName = dir.filePath( m_outputFileName );
   }

   QFile saveFile( fileName );
   saveFile.open(QIODevice::WriteOnly);
   saveFile.write(QJsonDocument(jsonObj).toJson());

   if(m_verbose)
       qDebug() << "wrote to file " << fileName;

   ui->statusBar->showMessage("Weigh scale data recorded.  Close when ready.");
}