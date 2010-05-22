/***************************************************************************
                   datasource.cpp  -  abstract data source
                             -------------------
    begin                : Thu Oct 16 2003
    copyright            : (C) 2003 The University of Toronto
    email                :
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "datasource.h"

#include <assert.h>

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLibraryInfo>
#include <QPluginLoader>
#include <QTextDocument>
#include <QUrl>
#include <QXmlStreamWriter>
#include <QTimer>
#include <QFileSystemWatcher>

#include "kst_i18n.h"
#include "datacollection.h"
#include "debug.h"
#include "objectstore.h"
#include "scalar.h"
#include "string.h"
//#include "stdinsource.h"
#include "updatemanager.h"

#include "dataplugin.h"

// TODO DataSource should not need the plugin code
#include "datasourcepluginmanager.h"

#define DATASOURCE_UPDATE_TIMER_LENGTH 1000


using namespace Kst;

template<class T>
struct NotSupportedImp : public DataSource::DataInterface<T>
{
  // read one element
  int read(const QString&, const typename T::Param&) { return -1; }

  // named elements
  QStringList list() const { return QStringList(); }
  bool isListComplete() const { return false; }
  bool isValid(const QString&) const { return false; }

  // T specific
  const typename T::Optional optional(const QString&) const { return typename T::Optional(); }
  void setOptional(const QString&, const typename T::Optional&) {}

  // meta data
  QMap<QString, double> metaScalars(const QString&) { return QMap<QString, double>(); }
  QMap<QString, QString> metaStrings(const QString&) { return QMap<QString, QString>(); }
};


const QString DataSource::staticTypeString = I18N_NOOP("Data Source");
const QString DataSource::staticTypeTag = I18N_NOOP("source");


Object::UpdateType DataSource::objectUpdate(qint64 newSerial) {
  if (_serial==newSerial) {
    return NoChange;
  }

  UpdateType updated = NoChange;

  if (!UpdateManager::self()->paused()) {
    // update the datasource
    updated = internalDataSourceUpdate();

    if (updated == Updated) {
      _serialOfLastChange = newSerial; // tell data objects it is new
    }
  }

  _serial = newSerial;

  return updated;
}


void DataSource::_initializeShortName() {
}

bool DataSource::isValid() const {
  return _valid;
}


bool DataSource::hasConfigWidget() const {
    return DataSourcePluginManager::sourceHasConfigWidget(_filename, fileType());
}


DataSourceConfigWidget* DataSource::configWidget() {
  if (!hasConfigWidget())
    return 0;

  DataSourceConfigWidget *w = DataSourcePluginManager::configWidgetForSource(_filename, fileType());
  Q_ASSERT(w);

  //This is still ugly to me...
  w->_instance = this;
  w->load();
  return w;
}



DataSource::DataSource(ObjectStore *store, QSettings *cfg, const QString& filename, const QString& type) :
  Object(),
  _filename(filename),
  _cfg(cfg),
  _updateCheckType(File),
  interf_scalar(new NotSupportedImp<DataScalar>),
  interf_string(new NotSupportedImp<DataString>),
  interf_vector(new NotSupportedImp<DataVector>),
  interf_matrix(new NotSupportedImp<DataMatrix>)
{
  Q_UNUSED(type)
  Q_UNUSED(store)
  _valid = false;
  _reusable = true;
  _writable = false;
  _watcher = 0L;

  QString shortFilename = filename;
  while (shortFilename.at(shortFilename.length() - 1) == '/') {
    shortFilename.truncate(shortFilename.length() - 1);
  }
  shortFilename = shortFilename.section('/', -1);
  QString tn = i18n("DS-%1", shortFilename);
  _shortName = tn;

  if (_updateCheckType == Timer) {
    QTimer::singleShot(UpdateManager::self()->minimumUpdatePeriod()-1, this, SLOT(checkUpdate()));
  } else if (_updateCheckType == File) {
    _watcher = new QFileSystemWatcher();
    _watcher->addPath(_filename);
    connect(_watcher, SIGNAL(fileChanged ( const QString & )), this, SLOT(checkUpdate()));
    connect(_watcher, SIGNAL(directoryChanged ( const QString & )), this, SLOT(checkUpdate()));
  }
}


DataSource::~DataSource() {
  if (_watcher) {
    disconnect(_watcher, SIGNAL(fileChanged ( const QString & )), this, SLOT(checkUpdate()));
    disconnect(_watcher, SIGNAL(directoryChanged ( const QString & )), this, SLOT(checkUpdate()));
    delete _watcher;
    _watcher = 0L;
  }

  delete interf_scalar;
  delete interf_string;
  delete interf_vector;
  delete interf_matrix;
}


void DataSource::setInterface(DataInterface<DataScalar>* i) {
  delete interf_scalar;
  interf_scalar = i;
}

void DataSource::setInterface(DataInterface<DataString>* i) {
  delete interf_string;
  interf_string = i;
}

void DataSource::setInterface(DataInterface<DataVector>* i) {
  delete interf_vector;
  interf_vector = i;
}

void DataSource::setInterface(DataInterface<DataMatrix>* i) {
  delete interf_matrix;
  interf_matrix = i;
}



void DataSource::setUpdateType(UpdateCheckType updateType)
{
_updateCheckType = updateType;
}


void DataSource::checkUpdate() {
  if (!UpdateManager::self()->paused()) {
    UpdateManager::self()->doUpdates(false);
  }

  if (_updateCheckType == Timer) {
    QTimer::singleShot(UpdateManager::self()->minimumUpdatePeriod()-1, this, SLOT(checkUpdate()));
  }
}


void DataSource::deleteDependents() {
}


const QString& DataSource::typeString() const {
  return staticTypeString;
}





QString DataSource::fileName() const {
  // Look to see if it was a URL and save the URL instead
  const QMap<QString,QString> urlMap = DataSourcePluginManager::urlMap();
  for (QMap<QString,QString>::ConstIterator i = urlMap.begin(); i != urlMap.end(); ++i) {
    if (i.value() == _filename) {
      return i.key();
    }
  }
  return _filename;
}











QString DataSource::fileType() const {
  return QString::null;
}

void DataSource::save(QXmlStreamWriter &s) {
  Q_UNUSED(s)
}


void DataSource::saveSource(QXmlStreamWriter &s) {
  QString name = _filename;
  // Look to see if it was a URL and save the URL instead
  const QMap<QString,QString> urlMap = DataSourcePluginManager::urlMap();
  for (QMap<QString,QString>::ConstIterator i = urlMap.begin(); i != urlMap.end(); ++i) {
    if (i.value() == _filename) {
      name = i.key();
      break;
    }
  }
  s.writeStartElement("source");
  s.writeAttribute("reader", fileType());
  s.writeAttribute("file", name);
  save(s);
  s.writeEndElement();
}



void DataSource::parseProperties(QXmlStreamAttributes &properties) {
  Q_UNUSED(properties);
}


void *DataSource::bufferMalloc(size_t size) {
  return malloc(size);
}


void DataSource::bufferFree(void *ptr) {
  return ::free(ptr);
}


void *DataSource::bufferRealloc(void *ptr, size_t size) {
  return realloc(ptr, size);
}





bool DataSource::isEmpty() const {
  return true;
}


void DataSource::reset() {
  Object::reset();
}


bool DataSource::supportsTimeConversions() const {
  return false;
}


int DataSource::sampleForTime(const QDateTime& time, bool *ok) {
  Q_UNUSED(time)
  if (ok) {
    *ok = false;
  }
  return 0;
}



int DataSource::sampleForTime(double ms, bool *ok) {
  Q_UNUSED(ms)
  if (ok) {
    *ok = false;
  }
  return 0;
}



QDateTime DataSource::timeForSample(int sample, bool *ok) {
  Q_UNUSED(sample)
  if (ok) {
    *ok = false;
  }
  return QDateTime::currentDateTime();
}



double DataSource::relativeTimeForSample(int sample, bool *ok) {
  Q_UNUSED(sample)
  if (ok) {
    *ok = false;
  }
  return 0;
}


bool DataSource::reusable() const {
  return _reusable;
}


void DataSource::disableReuse() {
  _reusable = false;
}

QString DataSource::_automaticDescriptiveName() const {
  return fileName();
}

QString DataSource::descriptionTip() const {
  return fileName();
}

/////////////////////////////////////////////////////////////////////////////
DataSourceConfigWidget::DataSourceConfigWidget(QSettings& settings)
: QWidget(0L), _cfg(settings) {
}


DataSourceConfigWidget::~DataSourceConfigWidget() {
}


void DataSourceConfigWidget::setInstance(DataSourcePtr inst) {
  _instance = inst;
}


DataSourcePtr DataSourceConfigWidget::instance() const {
  return _instance;
}


QSettings& DataSourceConfigWidget::settings() const {
  return _cfg;
}


bool DataSourceConfigWidget::hasInstance() const {
  return _instance != 0L;
}


ValidateDataSourceThread::ValidateDataSourceThread(const QString& file, const int requestID) : QRunnable(),
  _file(file),
  _requestID(requestID) {
}


void ValidateDataSourceThread::run() {
  QFileInfo info(_file);
  if (!info.exists()) {
    emit dataSourceInvalid(_requestID);
    return;
  }

  if (!DataSourcePluginManager::validSource(_file)) {
    emit dataSourceInvalid(_requestID);
    return;
  }

  emit dataSourceValid(_file, _requestID);
}


// vim: ts=2 sw=2 et


