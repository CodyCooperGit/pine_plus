#ifndef WEIGHSCALETEST_H
#define WEIGHSCALETEST_H

#include "TestBase.h"
#include "WeightMeasurement.h"

/*!
 * \class WeighScaleTest
 * \brief A Rice Lake digital weigh scale test class
 *
 * Concrete implementation of TestBase using WeightMeasurement
 * class specialization.  This class parses the binary data stream
 * obtained as a QByteArray from the RS232 interface controlled by the WeighScaleManager
 * class.
 *
 * \sa WeighScaleManager, WeightMeasurement, MeasurementBase
 *
 */

class WeighScaleTest : public TestBase<WeightMeasurement>
{
public:
    WeighScaleTest() = default;
    ~WeighScaleTest() = default;

    void fromArray(const QByteArray &);

    // String representation for debug and GUI display purposes
    //
    QString toString() const override;

    bool isValid() const override;

    // String keys are converted to snake_case
    //
    QJsonObject toJsonObject() const override;

};

Q_DECLARE_METATYPE(WeighScaleTest);

#endif // WEIGHSCALETEST_H