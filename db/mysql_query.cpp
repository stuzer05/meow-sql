#include <QDebug>
#include "mysql_query.h"
#include "editable_grid_data.h"
#include "data_type/mysql_data_type.h"

namespace meow {
namespace db {

// To distinguish between binary and nonbinary data for string data types,
// check whether the charsetnr value is 63.
static const int MYSQL_BINARY_CHARSET_NUMBER = 63;

MySQLQuery::MySQLQuery(Connection *connection)
    :Query(connection),
     _curRow(nullptr)
{

}

MySQLQuery::~MySQLQuery()
{

}

void MySQLQuery::execute(bool addResult /*= false*/) // override
{
    qDebug() << "[MySQLQuery] " << "Executing: " << SQL();

    // TODO addResult for isEditing() is broken

    QueryResults results = connection()->query(this->SQL(), true);

    MySQLQueryResultPt lastResult = nullptr;
    if (!results.isEmpty()) {
        lastResult = std::static_pointer_cast<MySQLQueryResult>(results.front());
    }

    if (addResult && _resultList.size() == 0) {
        addResult = false;
    }

    if (addResult == false) {
        _resultList.clear();
        _recordCount = 0;
        // H: ...
        if (isEditing()) {
            _editableData->clear();
        }
    }

    if (lastResult) {
        _resultList.push_back(lastResult);
        _recordCount += lastResult->nativePtr()->row_count;
    }

    if (!addResult) {
        _columns.clear();
        _columnLengths.clear();
        _columnIndexes.clear();
        if (_resultList.empty() == false) {
            // FCurrentResults is normally done in SetRecNo, but never if result has no rows
            // H: FCurrentResults := LastResult;

            unsigned int numFields = mysql_num_fields(lastResult->nativePtr());

            _columnLengths.resize(numFields);
            // TODO: skip columns parsing when we don't need them (e.g. sample queries)

            _columns.resize(numFields);

            for (unsigned int i=0; i < numFields; ++i) {
                MYSQL_FIELD * field = mysql_fetch_field_direct(lastResult->nativePtr(), i);
                QueryColumn & column = _columns[i];
                QString fieldName = QString(field->name);
                column.name = fieldName;
                _columnIndexes.insert(fieldName, i);
                if (connection()->serverVersionInt() >= 40100) {
                    column.orgName = QString(field->org_name);
                } else {
                    column.orgName = fieldName;
                }
                column.flags = field->flags;
                column.dataTypeIndex = dataTypeOfField(field);
                column.dataTypeCategoryIndex
                    = meow::db::categoryOfDataType(column.dataTypeIndex);
            }

            if (isEditing()) {
                prepareResultForEditing(lastResult->nativePtr());
                // TODO: free native result?
            }

            seekFirst();
        }
    }
}

DataTypeIndex MySQLQuery::dataTypeOfField(MYSQL_FIELD * field)
{
    // http://dev.mysql.com/doc/refman/5.7/en/c-api-data-structures.html

    bool isStringType = field->type == FIELD_TYPE_STRING;

    // ENUM and SET values are returned as strings. For these, check that
    // the type value is MYSQL_TYPE_STRING and that the ENUM_FLAG or SET_FLAG
    // flag is set in the flags value.
    if (isStringType) {
        if (field->flags & ENUM_FLAG) {
            return DataTypeIndex::Enum;
        } else if (field->flags & SET_FLAG) {
            return DataTypeIndex::Set;
        }
    }

    // To distinguish between binary and nonbinary data for string data types,
    // check whether the charsetnr value is 63. If so, the character set is
    // binary, which indicates binary rather than nonbinary data. This enables
    // you to distinguish BINARY from CHAR, VARBINARY from VARCHAR, and the BLOB
    // types from the TEXT types.
    bool isBinary;
    if (connection()->isUnicode()) {
        isBinary = field->charsetnr == MYSQL_BINARY_CHARSET_NUMBER;
    } else { // TODO: not sure we need this
        isBinary = (field->flags & BINARY_FLAG);
    }

    return dataTypeFromMySQLDataType(field->type, isBinary);
}


bool MySQLQuery::hasResult()
{
    return _resultList.empty() == false;
}

void MySQLQuery::seekRecNo(db::ulonglong value)
{
    if (value == _curRecNo) {
        return;
    }

    if (value >= recordCount()) {
        _curRecNo = recordCount();
        _eof = true;
        return;
    }

    if (isEditing() == false) {
        db::ulonglong numRows = 0;
        for (auto result : _resultList) {
            numRows += result->nativePtr()->row_count;
            if (numRows > value) {
                _currentResult = result; // TODO: why ?
                MYSQL_RES * curResPtr = _currentResult->nativePtr();
                // TODO: using unsigned with "-" is risky
                db::ulonglong wantedLocalRecNo = curResPtr->row_count - (numRows - value);
                // H: Do not seek if FCurrentRow points to the previous row of the wanted row
                if (wantedLocalRecNo == 0 || (_curRecNo+1 != value) || _curRow == nullptr) {
                    // TODO: it does not seems we need 3rd condition?
                    mysql_data_seek(curResPtr, wantedLocalRecNo);
                }

                _curRow = mysql_fetch_row(curResPtr);

                // H: Remember length of column contents. Important for Col() so contents
                // of cells with #0 chars are not cut off
                unsigned long * lengths = mysql_fetch_lengths(curResPtr);

                for (size_t i=0; i<_columnLengths.size(); ++i) {
                    _columnLengths[i] = lengths[i];
                }

                break;
            }
        }
    }

    _curRecNo = value;
    _eof = false;
}

QString MySQLQuery::curRowColumn(std::size_t index, bool ignoreErrors)
{
    if (index < columnCount()) {

        if (isEditing()) {
            return _editableData->dataAt(_curRecNo, index);
        }

        return rowDataToString(_curRow, index, _columnLengths[index]);

    } else if (!ignoreErrors) {
        throw db::Exception(
            QString("Column #%1 not available. Query returned %2 columns and %3 rows.")
                    .arg(index).arg(columnCount()).arg(recordCount())
        );
    }

    return QString();
}

bool MySQLQuery::isNull(std::size_t index)
{
    throwOnInvalidColumnIndex(index);

    if (isEditing()) {
        return _editableData->dataAt(_curRecNo, index).isNull();
    }

    return _curRow[index] == nullptr;
}

void MySQLQuery::throwOnInvalidColumnIndex(std::size_t index)
{
    if (index >= columnCount()) {
        throw db::Exception(
            QString("Column #%1 not available. Query returned %2 columns.")
                    .arg(index).arg(columnCount())
        );
    }
}

QString MySQLQuery::rowDataToString(MYSQL_ROW row,
                        std::size_t col,
                        unsigned long dataLen)
{
    QString result;

    DataTypeCategoryIndex typeCategory = column(col).dataTypeCategoryIndex;
    if (typeCategory == DataTypeCategoryIndex::Binary
        || typeCategory == DataTypeCategoryIndex::Spatial) {
        result = QString::fromLatin1(row[col], dataLen);
    } else {
        // TODO: non-unicode support?
        result = QString::fromUtf8(row[col], dataLen);
    }

    return result;
}

void MySQLQuery::prepareResultForEditing(MYSQL_RES * result)
{
    // it seems that copying all data is simplest way as we need to
    // insert/delete rows at top/in the middle of data as well
    // TODO: heidi works other way, maybe it's faster and/or takes less memory

    db::ulonglong numRows = result->row_count;
    unsigned int numCols = mysql_num_fields(result);

    _editableData->reserveForAppend(numRows);

    MYSQL_ROW rowDataRaw;
    while ((rowDataRaw = mysql_fetch_row(result))) {
        GridDataRow rowData;
        rowData.reserve(numCols);

        unsigned long * lengths = mysql_fetch_lengths(result);

        for (unsigned col = 0; col < numCols; ++col) {
            rowData.append(
                rowDataToString(rowDataRaw, col, lengths[col])
            );
        }

        _editableData->appendRow(rowData);
    }
}

} // namespace db
} // namespace meow

