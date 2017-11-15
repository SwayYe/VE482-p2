# LemonDB

## Introduction

A simple multi-thread key-value database by Lemonion. Inc. 

See more information in our official documentation [HTML](https://tc-imba.github.io/VE482-p2/)/[PDF](../latex/refman.pdf)

## Compilation

### Debug
This version is used for debugging.
```
mkdir debug && cd debug
cmake ..  -DCMAKE_BUILD_TYPE=Debug
make lemondb
src/lemondb
```

### Release
This version is used for performance test.
```
mkdir release && cd release
cmake ..  -DCMAKE_BUILD_TYPE=Release
make lemondb
src/lemondb
```

## Testing
For a small test case, just use the files under `test` folder. Set the working directory as `test`, set the program argument as `test.query` or `test*.in`.

The test cases are too bug, so they are stored with Git LFS. See more information on [Git LFS pages](https://git-lfs.github.com/).

Once Git LFS extension is installed, you can download the test cases through cloning the submodule:
```
git submodule init
git submodule update
```

Set the working directory as `testcase/sample`, set the program argument as `*.query`, simply start debugging!
(The loading query in all test files should be based on `testcase/sample` directory)

## Documentation

The working flow of LemonDB is written by ourselves.

The class / function documentation is generated by [Doxygen](http://www.doxygen.nl/).

### Design
* We design this program to make it can create 8 threads. We classify the queries using table and add them into queryqueue of corresponding table when we read them from the file. Once the table needed has been loaded completely, we will execute the queries as the order in queue and read query if the file hasn't ended at the same time.
* Queries in the queryqueue will be run in the parallel.
* Even in one query, for data queries that need searching and calculation such as SUB or SUM in a large table, we use a function addIterationTask to divide the table into several parts and search or calculate these parts simultaneously. Because the efficiency depends on the ratio between size of every part and size of the table, we did experiments and then find 10000 is a good size. For queries like TRUNCATE and INSERT, since they don't have to traverse the whole table, we don't add task for them.
* The query class will use a "combine" function to check whether all the tasks dispatched by one query are all finished and organize them in the order and show on the screen.
* When the user ask for quit, the quit query will check whether all tasks have already finished and and exit the program.

### Performance Improvement
We use many tricks to improve performance:
* Since we use a vector to store all data in a table, we obtain the advantage of efficient random access. Meanwhile, deleting and inserting datum becomes less efficient inevitably. However, we use some tricks to handle this issue. Notice that the vector is unordered, so for INSERT query it can be trivially appended to the vector with O(1). Then for DELETE and DUPLICATE query, we use a temporary vector `dataNew`. When iterating through data, those won't be deleted will be moved to dataNew by `std::move`, which is extremely fast. Then we simply swap `data` and `dataNew`, clear `dataNew` for further use. For DUPLICATE, things are slightly different. We insert duplicated data into `dataNew`, and then we append `dataNew` to `data`. These iteration can be executed in parallel, so `dataNew` is, of course, protected by a mutex.
* Another great improvement is for those query with a condition 'KEY = someKey'. Making use of efficient random access, we keep a `keyMap` which stores index for given key. With this map, we can complete those query very efficiently, without iterating through data.
* For those query without given key, we also improve the speed of evaluating condition. This is done by computing the condition explicitly for a specific query (by `std::function`), and simply pass this function to it. By doing so, we don't need to repeatedly compare string, convert string to integer, even switch among operators. This can save huge amount of time, because originally every datum use one general evaluating function. Now we just need to compute it once per query.
* In some trivial cases atomic_int is used instead of having a mutex because it is much faster.

### Problems Solved

Due to our sophisticated design, we ran into many problems. These are some of them:
* We have encountered many problems about the query queue. The problem of LOAD query is the most difficult one, because it doesn't specify a table name. In our design, every table has a query queue so that we can decide the order to execute them, following reader/writer pattern. But LOAD doesn't have it, so it's very difficult to deicide where to put it, because the file may even not exist when the query is parsed and put into some queue. DUMP query is the reason we concern about this issue, so we solve it by keeping a map from filename to tablename. With this map, LOAD can decide whether the file is (or will be) created by DUMP or should exist already.
* Another issue is COPYTABLE. This is the other query which can create a table, in which case it is responsible for starting the query queue to execute. And the problem is that COPYTABLE involves 2 table, so it should be pushed to both tables. And only when both query queues come to this query should it execute. This is done by keeping each other's pointer in it.
* Other problems such as mutex or deadlock are less encountered, because we consider carefully about implementation before we start to code.