#include "StatisticsThread.h"
#include <QFileInfo>
#include <QDir>
#include <QFileInfoList>
#include <QDebug>
#include <QMessageBox>

void StatisticsThread::run(){
    running = true;
    while(Q.size()){
        qDebug() << "Queue Item started";
        QPair<QModelIndex, runID_t> p = Q.front();
        switch(p.second){
        case ID_dirSize:
            _dirSize(p.first);
            emit dirSizeSignal(p.first);
            break;
        case ID_getExt:
            _getExt(p.first);
            emit getExtSignal(p.first);
            break;
        case ID_getOwn:
            _getOwn(p.first);
            emit getOwnSignal(p.first);
            break;
        case ID_getGroup:
            _getGroup(p.first);
            emit getGroupSignal(p.first);
            break;
        default:
            break;
        }
        Q.dequeue();
        qDebug() << "Queue Item finised";
    }
    running = false;
    qDebug() << "Thread is done";
}
StatisticsThread::StatisticsThread(QFileSystemModel *ptr){
    fsModel = ptr;
    running = false;
    //QThread::start();
}

QJsonObject StatisticsThread::getJson(QModelIndex idx,qint32 depth)
{
    QFileInfo fInfo = fsModel->fileInfo(idx);
    if(fInfo.isFile())
    {
        QJsonObject temp;
        temp.insert("name",QJsonValue(fInfo.fileName()));
        temp.insert("size",QJsonValue(fInfo.size()));
        return temp;
    }
    if(depth>=MAX_DEPTH){

        QJsonObject temp;
        temp.insert("name",QJsonValue(fInfo.fileName()));
        temp.insert("size",QJsonValue((long long) _dirSize(idx)));
        return temp;
    }
    QJsonObject obj;
    QJsonArray children;

    QDir dInfo (fInfo.absoluteFilePath());
    QFileInfoList dList = dInfo.entryInfoList(QDir::Files | QDir::Dirs |  QDir::NoDotAndDotDot | QDir::NoSymLinks | QDir::Hidden) ;
    for(int i = 0; i < dList.size(); i++)
    {
            children.append(getJson(fsModel->index(dList[i].absoluteFilePath()),depth+1));

    }
    obj["name"]=fInfo.fileName()+"Dir";
    obj["children"]=children;
    return obj;
}

quint64 StatisticsThread::_dirSize(QModelIndex idx){
    QFileInfo fInfo = fsModel->fileInfo(idx);
    if(fInfo.isFile()) return fInfo.size();
    if(mpSize.contains(idx)) return mpSize[idx];
    quint64& ret = mpSize[idx];
    ret = 0;
    
    QDir dInfo (fInfo.absoluteFilePath());
    QFileInfoList dList = dInfo.entryInfoList(QDir::Files | QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot | QDir::NoSymLinks) ;//| QDir::Hidden | QDir::System);
    for(int i = 0; i < dList.size(); i++)
        ret += _dirSize(fsModel->index(dList[i].absoluteFilePath())); 
    
    qDebug() << "dirSize\t" << fInfo.absoluteFilePath();
    return ret;
}
quint64 StatisticsThread::fileCount(QModelIndex idx){
    QFileInfo fInfo = fsModel->fileInfo(idx);
    if(fInfo.isFile()) return 1;
    if(mpNExec.contains(idx)) return mpNExec[idx];
    
    quint64& ret = mpNExec[idx]; ret = 0;
    QDir dInfo(fInfo.absoluteFilePath());
    QFileInfoList dList = dInfo.entryInfoList(QDir::Files | QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    for(int i = 0; i < dList.size(); i++)
        ret += nExec(fsModel->index(dList[i].absoluteFilePath()));
    qDebug() << "fileCount\t" << fInfo.absoluteFilePath();
    return ret; 
}
quint64 StatisticsThread::nExec(QModelIndex idx){
    //Returns number of Executable files in a given directory.
    QFileInfo fInfo = fsModel->fileInfo(idx);
    if(fInfo.isFile() && fInfo.isExecutable()) {
        fiLExec.append(fInfo);
        return 1;
    }
    if(fInfo.isFile() && !fInfo.isExecutable()) return 0;
    if(mpNExec.contains(idx)) return mpNExec[idx];
    
    quint64& ret = mpNExec[idx]; ret = 0;
    QDir dInfo(fInfo.absoluteFilePath());
    QFileInfoList dList = dInfo.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    for(int i = 0; i < dList.size(); i++)
        ret += nExec(fsModel->index(dList[i].absoluteFilePath()));
    qDebug() << "nExec\t" << fInfo.absoluteFilePath();
    return ret;
}
const StatisticsThread::ExtStat* const StatisticsThread::_getExt(QModelIndex idx){
    //Returns statistics about the files inside a given directory.
    QFileInfo fInfo = fsModel->fileInfo(idx);
    if(fInfo.isFile()) return 0;
    if(mpExt.contains(idx)) return &mpExt[idx];
    
    ExtStat& ret = mpExt[idx];
    
    QDir dInfo (fInfo.absoluteFilePath());
    QFileInfoList fList = dInfo.entryInfoList(QDir::Files | QDir::Hidden | QDir::NoSymLinks | QDir::NoDotAndDotDot);
    for(int i = 0; i < fList.size(); i++) ret.nExt[fList[i].suffix()]++;
    QFileInfoList dList = dInfo.entryInfoList(QDir::Dirs | QDir::Hidden | QDir::NoSymLinks | QDir::NoDotAndDotDot);
    for(int i = 0; i < dList.size(); i++){
        _getExt(fsModel->index(dList[i].absoluteFilePath()));
        ExtStat& r = mpExt[fsModel->index(dList[i].absoluteFilePath())];
        for(QMap<QString, quint64>::iterator it = r.nExt.begin(); it != r.nExt.end(); it++)
            ret.nExt[it.key()] += it.value();
    }
    qDebug() << "getExt\t" << fInfo.absoluteFilePath();
    return &mpExt[idx];
}
const StatisticsThread::OwnStat* const StatisticsThread::_getOwn(QModelIndex idx){
    QFileInfo fInfo = fsModel->fileInfo(idx);
    if(fInfo.isFile()){
        OwnRet.nOwn.clear();
        OwnRet.nOwn[fInfo.owner()] = 1;
        return &OwnRet;
    }
    if(mpOwn.contains(idx)) return &mpOwn[idx];
    
    OwnStat& ret = mpOwn[idx];
    
    QDir dInfo(fInfo.absoluteFilePath());
    QFileInfoList fList = dInfo.entryInfoList(QDir::Files | QDir::Hidden | QDir::NoSymLinks | QDir::NoDotAndDotDot);
    for(int i = 0; i < fList.size(); i++) ret.nOwn[fList[i].owner()]++;
    QFileInfoList dList = dInfo.entryInfoList(QDir::Dirs | QDir::Hidden | QDir::NoSymLinks | QDir::NoDotAndDotDot);
    for(int i = 0; i < dList.size(); i++){
        _getOwn(fsModel->index(dList[i].absoluteFilePath()));
        OwnStat& r = mpOwn[fsModel->index(dList[i].absoluteFilePath())];
        for(QMap<QString, quint64>::iterator it = r.nOwn.begin(); it != r.nOwn.end(); it++)
            ret.nOwn[it.key()] += it.value();
    }
    qDebug() << "getOwn\t" << fInfo.absoluteFilePath();
    return &mpOwn[idx];
}
const StatisticsThread::GroupStat* const StatisticsThread::_getGroup(QModelIndex idx){
    QFileInfo fInfo = fsModel->fileInfo(idx);
    if(fInfo.isFile()){
        GroupRet.nGroup.clear();
        GroupRet.nGroup[fInfo.owner()] = 1;
        return &GroupRet;
    }
    if(mpGroup.contains(idx)) return &mpGroup[idx];
    
    GroupStat& ret = mpGroup[idx];
    
    QDir dInfo(fInfo.absoluteFilePath());
    QFileInfoList fList = dInfo.entryInfoList(QDir::Files | QDir::Hidden | QDir::NoSymLinks | QDir::NoDotAndDotDot);
    for(int i = 0; i < fList.size(); i++) ret.nGroup[fList[i].group()]++;
    QFileInfoList dList = dInfo.entryInfoList(QDir::Dirs | QDir::Hidden | QDir::NoSymLinks | QDir::NoDotAndDotDot);
    for(int i = 0; i < dList.size(); i++){
        _getGroup(fsModel->index(dList[i].absoluteFilePath()));
        GroupStat& r = mpGroup[fsModel->index(dList[i].absoluteFilePath())];
        for(QMap<QString, quint64>::iterator it = r.nGroup.begin(); it != r.nGroup.end(); it++)
            ret.nGroup[it.key()] += it.value();
    }
    qDebug() << "getGroup\t" << fInfo.absoluteFilePath();
    return &mpGroup[idx];
}

void StatisticsThread::dirSize(QModelIndex idx){
    Q.enqueue(QPair<QModelIndex, runID_t>(idx, ID_dirSize));
    if(!running) QThread::start();
}
void StatisticsThread::getExt(QModelIndex idx){
    Q.enqueue(QPair<QModelIndex, runID_t>(idx, ID_getExt));
    if(!running) QThread::start();
}
void StatisticsThread::getOwn(QModelIndex idx){
    Q.enqueue(QPair<QModelIndex, runID_t>(idx , ID_getOwn));
    if(!running) QThread::start();
}
void StatisticsThread::getGroup(QModelIndex idx){
    Q.enqueue(QPair<QModelIndex, runID_t>(idx , ID_getGroup));
    if(!running) QThread::start();
}

QFileInfoList *StatisticsThread::lExec(){
    return &fiLExec;
}
void StatisticsThread::lExecClear(){
    fiLExec.clear();
}
bool StatisticsThread::isRunning() const{
    return running;
}
