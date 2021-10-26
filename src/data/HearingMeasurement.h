#ifndef HEARINGMEASUREMENT_H
#define HEARINGMEASUREMENT_H

#include "MeasurementBase.h"

/*!
 * \class HearingMeasurement
 * \brief A HearingMeasurement class
 *
 * \sa MeasurementBase
 */

class HearingMeasurement :  public MeasurementBase
{   
public:
    HearingMeasurement() = default;
    ~HearingMeasurement() = default;

    void fromCode(const QString &, const int &, const QString &);

    QString toString() const override;

    bool isValid() const override;

    static QMap<QString,QString> initCodeLookup();
    static QMap<QString,QString> initOutcomeLookup();
    static QMap<int,QString> initFrequencyLookup();

private:
    static QMap<QString,QString> codeLookup;
    static QMap<QString,QString> outcomeLookup;
    static QMap<int,QString> frequencyLookup;
};

Q_DECLARE_METATYPE(HearingMeasurement);

QDebug operator<<(QDebug dbg, const HearingMeasurement &);

#endif // HEARINGMEASUREMENT_H
