#ifndef QUERYHELPER_H
#define QUERYHELPER_H

#include <QObject>
#include <QSqlQuery>
#include <QJsonObject>

/*!
 * \class QueryHelper
 * \brief Helper class to query MS Excel files
 *
 * MS Excel files can be queried over ODBC driver
 * using the basic SELECT * FROM [SHEETNAME$CELL_j:CELL_k]
 * SQL statement.  The sheet name must be specified
 * and defaults to "Sheet1".  Cell ranges (eg, A1:BA3)
 * can be specified in lower or upper case and in any order.
 * Internally the class capitalizes starting and ending cells
 * such that the order is logical and satisfies the query
 * statement constraints.
 * Depending on the number of rows, columns and the optional
 * specification of a sting list of (row or column) header labels, data can
 * be retrieved and interpreted in json format (QJsonObject).
 *
 * Caveats:
 * The data Order must be set before setting a header.
 * The sheet name must be set before building the query.
 *
 * \sa CDTTTest
 *
 */

QT_FORWARD_DECLARE_CLASS(QSqlDatabase)

class QueryHelper
{
public:

    enum Order {
        Row,
        Column
    };

    enum HeaderMode {
        Detached,   // the header is not part of the expected data
        Integrated // the header is part of the expected data
    };

    QueryHelper(const QString&,
                const QString&,
                const QString&);
    QueryHelper(const QueryHelper &other);
    QueryHelper& operator=(const QueryHelper &other);

    bool buildQuery(const QSqlDatabase&);
    void processQuery();

    void setSheet(const QString&);
    void setHeader(const QStringList&);
    void setOrder(const QueryHelper::Order&);
    void setHeaderMode(const QueryHelper::HeaderMode&);

    const QJsonObject& getOutput() { return m_output; }

private:

    void initialize();
    int columnToIndex(const QString&);

    int n_row;
    int n_col;
    QString m_cellStart;
    QString m_cellEnd;
    QString m_sheet;
    QStringList m_header;
    Order m_order;
    HeaderMode m_mode;
    QSqlQuery m_query;
    QJsonObject m_output;
};

#endif // QUERYHELPER_H
