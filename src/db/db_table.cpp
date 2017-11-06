#include <algorithm>
#include "db_table.h"
#include "../uexception.h"
#include "../formatter.h"


Table::Ptr loadTableFromStream(std::istream &infile, std::string source) {
    std::string errString =
            source != "" ?
            R"(Invalid table (from "?") format: )"_f % source :
            "Invalid table format: ";

    std::string tableName;
    int fieldCount;
    std::vector<std::string> fields;

    std::string line;
    std::stringstream sstream;
    if (!std::getline(infile, line))
        throw LoadFromStreamException(
                errString + "Failed to read table metadata line."
        );

    sstream.str(line);
    sstream >> tableName >> fieldCount;
    if (!sstream) {
        throw LoadFromStreamException(
                errString + "Failed to parse table metadata."
        );
    }


    if (!(std::getline(infile, line)))
        throw LoadFromStreamException(
                errString + "Failed to load field names."
        );

    sstream.str("");
    sstream.str(line);
    sstream.clear();
    for (int i = 0; i < fieldCount; ++i) {
        std::string field;
        if (!(sstream >> field))
            throw LoadFromStreamException(
                    errString + "Failed to load field names."
            );
        else
            fields.push_back(field);
    }

    if (fields.front() != "KEY")
        throw LoadFromStreamException(
                errString + "Missing or invalid KEY field."
        );

    fields.erase(fields.begin()); // Remove leading key
    Table::Ptr table = std::make_unique<Table>(tableName, fields);

    int count = 2;
    while (std::getline(infile, line)) {
        if (line == "") break; // Read to an empty line
        count++;
        sstream.str("");
        sstream.clear();
        sstream.str(line);
        std::string key;
        if (!(sstream >> key))
            throw LoadFromStreamException(
                    errString + "Missing or invalid KEY field."
            );
        std::vector<int> tuple;
        for (int i = 1; i < fieldCount; ++i) {
            int value;
            if (!(sstream >> value))
                throw LoadFromStreamException(
                        errString + "Invalid row on LINE " + std::to_string(count)
                );
            tuple.push_back(value);
        }
        (*table).insertByIndex(key, tuple);
    }
    return table;
}

std::ostream &operator<<(std::ostream &os, const Table &table) {
    const int width = 10;
    std::stringstream buffer;
    buffer << table.tableName << "\t" << (table.fields.size() + 1) << "\n";
    buffer << std::setw(width) << "KEY";
    for (const auto &field : table.fields) {
        buffer << std::setw(width) << field;
    }
    buffer << "\n";
    int numFields = table.fields.size();
    for (const auto &datum : table.data) {
        buffer << std::setw(width) << datum.key;
        for (int i = 0; i < numFields; ++i) {
            buffer << std::setw(width) << datum.datum[i];
        }
        buffer << "\n";
    }
    return os << buffer.str();
}

void Table::addQuery(Query::Ptr &query) {
    queryQueueMutex.lock();
    if (queryQueueCounter < 0) {
        // writing, push back the query
        queryQueue.push_back(query);
        queryQueueMutex.unlock();
        return;
    }
    // idle/reading
    if (query->isWriter()) {
        // add a writer or execute it if idle
        if (queryQueueCounter == 0) {
            queryQueueCounter = -1;
            queryQueueMutex.unlock();
            query->execute();
        } else {
            queryQueue.push_back(query);
            queryQueueMutex.unlock();
        }
    } else {
        // add a reader and execute it at once
        ++queryQueueCounter;
        queryQueueMutex.unlock();
        query->execute();
    }

}

void Table::refreshQuery() {
    queryQueueMutex.lock();
    if (queryQueueCounter <= 0) {
        // writing or idle (should not happen), reset the counter
        queryQueueCounter = 0;
    } else {
        --queryQueueCounter;
    }
    if (queryQueueCounter == 0 && queryQueue.front()->isWriter()) {
        // if idle with a write query, execute it
        queryQueueCounter = -1;
        auto query = queryQueue.front();
        queryQueue.pop_front();
        queryQueueMutex.unlock();
        query->execute();
    } else {
        // if reading, execute all read query before next write query
        decltype(queryQueue) list;
        // STL may be a bit faster ?
        auto it = std::find_if(queryQueue.begin(), queryQueue.end(), [](const Query::Ptr &ptr) {
            return !ptr->isWriter();
        });
        //auto it = queryQueue.begin();
        //for (; it != queryQueue.end() && !(*it)->isWriter(); ++it) {}
        list.splice(list.begin(), queryQueue, queryQueue.begin(), it);
        queryQueueMutex.unlock();
        for (auto &item:list) {
            item->execute();
        }
    }
}
