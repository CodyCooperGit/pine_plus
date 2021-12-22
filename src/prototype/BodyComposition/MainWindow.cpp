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
    : QDialog(parent)
    , ui(new Ui::MainWindow)
    , m_verbose(false)
    , m_manager(this)
{
    ui->setupUi(this);
    // allocate 1 columns x 8 rows of hearing measurement items
    //
    for(int row=0;row<8;row++)
    {
      QStandardItem* item = new QStandardItem();
      m_model.setItem(row,0,item);

    }
    m_model.setHeaderData(0,Qt::Horizontal,"Body Composition Results",Qt::DisplayRole);
    ui->testdataTableView->setModel(&m_model);

    ui->testdataTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->testdataTableView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->testdataTableView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->testdataTableView->verticalHeader()->hide();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initialize()
{
  m_manager.setVerbose(m_verbose);
  m_manager.setMode(m_mode);

  QIntValidator *v_barcode = new QIntValidator(this);
  v_barcode->setRange(0,99999999);
  ui->ageLineEdit->setValidator(v_barcode);

  // Read inputs, such as interview barcode
  //
  readInput();

  // Populate barcode display
  //
  if(m_inputData.contains("barcode") && m_inputData["barcode"].isValid())
     ui->barcodeLineEdit->setText(m_inputData["barcode"].toString());
  else
     ui->barcodeLineEdit->setText("00000000"); // dummy

  // Read the .ini file for cached local and peripheral device addresses
  //
  QDir dir = QCoreApplication::applicationDirPath();
  QSettings settings(dir.filePath("bodycompanalyzer.ini"), QSettings::IniFormat);
  m_manager.loadSettings(settings);

  // Save button to store measurement and device info to .json
  //
  ui->saveButton->setEnabled(false);

  // Reset / clear the analyzer inputs
  //
  ui->resetButton->setEnabled(false);

  // Set the inputs to the analyzer
  //
  ui->setButton->setEnabled(false);

  // Read the measurements off the analyzer
  //
  ui->measureButton->setEnabled(false);

  // Confirm the inputs and enable measurement if confirmed
  //
  ui->confirmButton->setEnabled(false);

  // Connect to the device
  //
  ui->connectButton->setEnabled(false);

  // Disconnect from the device
  //
  ui->disconnectButton->setEnabled(false);

  // Close the application
  //
  ui->closeButton->setEnabled(true);

  // Scan for devices
  //
  connect(&m_manager, &TanitaManager::scanningDevices,
          this,[this]()
    {
      ui->deviceComboBox->clear();
      ui->statusBar->showMessage("Discovering serial ports...");
    }
  );

  // Update the drop down list as devices are discovered during scanning
  //
  connect(&m_manager, &TanitaManager::deviceDiscovered,
          this, &MainWindow::updateDeviceList);

  connect(&m_manager, &TanitaManager::deviceSelected,
          this,[this](const QString &label){
      if(label != ui->deviceComboBox->currentText())
      {
          ui->deviceComboBox->setCurrentIndex(ui->deviceComboBox->findText(label));
      }
  });

  // Prompt user to select a device from the drop down list when previously
  // cached device information in the ini file is unavailable or invalid
  //
  connect(&m_manager, &TanitaManager::canSelectDevice,
          this,[this](){
      ui->statusBar->showMessage("Ready to select...");
      QMessageBox::warning(
        this, QApplication::applicationName(),
        tr("Select the port from the list or connect to the currently visible port in the list.  If the device "
        "is not in the list, quit the application and check that the port is "
        "working and connect the analyzer to it before running this application."));
  });

  // Select a device (serial port) from the drop down list
  //
  connect(ui->deviceComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this,[this]()
    {
      if(m_verbose)
          qDebug() << "device selected from list " <<  ui->deviceComboBox->currentText();
      m_manager.selectDevice(ui->deviceComboBox->currentText());
    }
  );

  // Ready to connect device
  //
  connect(&m_manager, &TanitaManager::canConnectDevice,
          this,[this](){
      ui->statusBar->showMessage("Ready to connect...");
      ui->connectButton->setEnabled(true);
      ui->disconnectButton->setEnabled(false);
      ui->resetButton->setEnabled(false);
      ui->setButton->setEnabled(false);
      ui->confirmButton->setEnabled(false);
      ui->measureButton->setEnabled(false);
      ui->saveButton->setEnabled(false);
  });

  // Connect to device
  //
  connect(ui->connectButton, &QPushButton::clicked,
          &m_manager, &TanitaManager::connectDevice);

  // Connection is established, inputs are set and confirmed: enable measurement requests
  //
  connect(&m_manager, &TanitaManager::canMeasure,
          this,[this](){
      ui->statusBar->showMessage("Ready to measure...");
      ui->resetButton->setEnabled(true);
      ui->setButton->setEnabled(false);
      ui->measureButton->setEnabled(true);
      ui->confirmButton->setEnabled(true);
      ui->saveButton->setEnabled(false);
  });

  // Connection is established and a successful reset was done
  //
  connect(&m_manager, &TanitaManager::canInput,
          this,[this](){
      ui->statusBar->showMessage("Ready to accept inputs...");
      ui->connectButton->setEnabled(false);
      ui->disconnectButton->setEnabled(true);
      ui->resetButton->setEnabled(true);
      ui->setButton->setEnabled(true);
      ui->measureButton->setEnabled(false);
      ui->confirmButton->setEnabled(false);
      ui->saveButton->setEnabled(false);
  });

  // A successful confirmation of all inputs was done
  //
  connect(&m_manager, &TanitaManager::canConfirm,
          this,[this](){
      ui->statusBar->showMessage("Ready to confirm inputs ...");
      ui->connectButton->setEnabled(false);
      ui->disconnectButton->setEnabled(true);
      ui->resetButton->setEnabled(true);
      ui->setButton->setEnabled(true);
      ui->measureButton->setEnabled(false);
      ui->confirmButton->setEnabled(true);
      ui->saveButton->setEnabled(false);
  });

  // Disconnect from device
  //
  connect(ui->disconnectButton, &QPushButton::clicked,
          &m_manager, &TanitaManager::disconnectDevice);

  // Reset the device (clear all input settings)
  //
  connect(ui->resetButton, &QPushButton::clicked,
        &m_manager, &TanitaManager::resetDevice);

  // Set the inputs to the analyzer
  //
  connect(ui->setButton, &QPushButton::clicked,
           this,[this](){
      QMap<QString,QVariant> inputs;
      inputs["equation"] = "westerner";

      inputs["measurement system"] =
        "metricRadio" == ui->unitsGroup->checkedButton()->objectName() ?
          "metric" : "imperial";

      QString units = inputs["measurement system"].toString();

      inputs["gender"] =
        "maleRadio" == ui->genderGroup->checkedButton()->objectName() ?
          "male" : "female";

      QString s = ui->ageLineEdit->text().simplified();
      s = s.replace(" ","");
      inputs["age"] = s.toUInt();

      inputs["body type"] =
        "standardRadio" == ui->bodyTypeGroup->checkedButton()->objectName() ?
          "standard" : "athlete";

      s = ui->heightLineEdit->text().simplified();
      s = s.replace(" ","");
      inputs["height"] = "metric" == units ? s.toUInt() :  s.toDouble();

      s = ui->tareWeightLineEdit->text().simplified();
      s = s.replace(" ","");
      inputs["tare weight"] = s.toDouble();

      qDebug() << inputs;

      m_manager.setInputs(inputs);
  });

  // Confirm inputs and check if measurement can proceed
  //
  connect(ui->confirmButton, &QPushButton::clicked,
        &m_manager, &TanitaManager::confirmSettings);

  // Request a measurement from the device
  //
  connect(ui->measureButton, &QPushButton::clicked,
        &m_manager, &TanitaManager::measure);

  // Update the UI with any data
  //
  connect(&m_manager, &TanitaManager::dataChanged,
          this,[this](){
      m_manager.buildModel(&m_model);
      int nrow = m_model.rowCount();
      QSize ts_pre = ui->testdataTableView->size();
      ui->testdataTableView->setColumnWidth(0,ui->testdataTableView->size().width()-2);
      ui->testdataTableView->resize(
                  ui->testdataTableView->width(),
                  nrow*(ui->testdataTableView->rowHeight(0)+1)+
                  ui->testdataTableView->horizontalHeader()->height());
      QSize ts_post = ui->testdataTableView->size();
      int dx = ts_post.width()-ts_pre.width();
      int dy = ts_post.height()-ts_pre.height();
      this->resize(this->width()+dx,this->height()+dy);
  });

  // All measurements received: enable write test results
  //
  connect(&m_manager, &TanitaManager::canWrite,
          this,[this](){
      ui->statusBar->showMessage("Ready to save results...");
      ui->saveButton->setEnabled(true);
  });

  QIntValidator *v_age = new QIntValidator(this);
  v_age->setRange(7,99);
  ui->ageLineEdit->setValidator(v_age);

  // When the units are changed to imperial, the input field for
  // height input must change to accomodate 0.5" increments
  //
  connect(ui->unitsGroup, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked),
          this,[this](QAbstractButton *button){
     ui->heightLineEdit->setText("");
     if("metricRadio" == button->objectName())
     {
         QIntValidator *v_ht = new QIntValidator(this);
         v_ht->setRange(90,249);
         ui->heightLineEdit->setValidator(v_ht);
     }
     else
     {
         QDoubleValidator *v_ht = new QDoubleValidator(this);
         v_ht->setRange(36.0,7*12 + 11.5);
         v_ht->setDecimals(1);
         ui->heightLineEdit->setValidator(v_ht);
     }
  });

  connect(&m_manager, &TanitaManager::error,
          this, [this](const QString &error){
      ui->statusBar->showMessage(error);
      QMessageBox::warning(
        this, QApplication::applicationName(),
        tr("A fatal error occured while attempting a measurement and the "
           "device was disconnected. Turn the device on if it is off, re-connect, "
           "re-input and confirm the inputs."));
  });

  // Write test data to output
  //
  connect(ui->saveButton, &QPushButton::clicked,
    this, &MainWindow::writeOutput);

  // Close the application
  //
  connect(ui->closeButton, &QPushButton::clicked,
          this, &MainWindow::close);

  emit m_manager.dataChanged();
}

void MainWindow::updateDeviceList(const QString &label)
{
    // Add the device to the list
    //
    int index = ui->deviceComboBox->findText(label);
    bool oldState = ui->deviceComboBox->blockSignals(true);
    if(-1 == index)
    {
        ui->deviceComboBox->addItem(label);
    }
    ui->deviceComboBox->blockSignals(oldState);
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
    QSettings settings(dir.filePath("bodycompanalyzer.ini"), QSettings::IniFormat);
    m_manager.saveSettings(&settings);
    m_manager.disconnectDevice();
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
         list << "bodycomp.json";
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

   ui->statusBar->showMessage("Body composition data recorded.  Close when ready.");
}