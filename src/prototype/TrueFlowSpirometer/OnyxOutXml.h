#pragma once

#include <QXmlStreamReader>
#include <QString>

#include "Models/OutDataModel.h"
#include "Models/TrialDataModel.h"

class OnyxOutXml
{
public:
	static OutDataModel readImportantValues(const QString &transferOutPath);
private:
	static void skipToEndElement(QXmlStreamReader* reader, const QString &name);
	static QString readCommand(QXmlStreamReader* reader);
	static void readPatients(QXmlStreamReader* reader, OutDataModel* outData, const QString& patientId);
	static void readPatient(QXmlStreamReader* reader, OutDataModel* outData);
	static void readIntervals(QXmlStreamReader* reader, OutDataModel* outData);
	static void readInterval(QXmlStreamReader* reader, OutDataModel* outData);
	static void readTests(QXmlStreamReader* reader, OutDataModel* outData);
	static void readFVCTest(QXmlStreamReader* reader, OutDataModel* outData);
	static void readPatientDataAtTestTime(QXmlStreamReader* reader, OutDataModel* outData);
	static void readTrials(QXmlStreamReader* reader, OutDataModel* outData);
	static void readTrial(QXmlStreamReader* reader, OutDataModel* outData);
	static ResultParametersModel readResultParameters(QXmlStreamReader* reader, const QString& closingTagName);
	static ResultParameterModel readResultParameter(QXmlStreamReader* reader);
	static void readChannelFlow(QXmlStreamReader* reader, TrialDataModel* outData);
	static void readChannelVolume(QXmlStreamReader* reader, TrialDataModel* outData);
};

