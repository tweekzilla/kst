/***************************************************************************
 *                                                                         *
 *   copyright : (C) 2007 The University of Toronto                        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "matrixfactory.h"

#include "debug.h"
#include "matrix.h"
#include "generatedmatrix.h"
#include "editablematrix.h"
#include "datamatrix.h"
#include "objectstore.h"

namespace Kst {

GeneratedMatrixFactory::GeneratedMatrixFactory()
: PrimitiveFactory() {
  registerFactory(GeneratedMatrix::staticTypeTag, this);
}


GeneratedMatrixFactory::~GeneratedMatrixFactory() {
}


PrimitivePtr GeneratedMatrixFactory::generatePrimitive(ObjectStore *store, QXmlStreamReader& xml) {
  ObjectTag tag;
  QByteArray data;

  Q_ASSERT(store);

  bool xDirection;
  double gradZMin, gradZMax, minX, minY, nX, nY, stepX, stepY;

  while (!xml.atEnd()) {
      const QString n = xml.name().toString();
    if (xml.isStartElement()) {
      if (n == GeneratedMatrix::staticTypeTag) {
        QXmlStreamAttributes attrs = xml.attributes();
        tag = ObjectTag::fromString(attrs.value("tag").toString());
        gradZMin = attrs.value("gradzmin").toString().toDouble();
        gradZMax = attrs.value("gradzmax").toString().toDouble();
        minX = attrs.value("xmin").toString().toDouble();
        minY = attrs.value("ymin").toString().toDouble();
        nX = attrs.value("nx").toString().toDouble();
        nY = attrs.value("ny").toString().toDouble();
        stepX = attrs.value("xstep").toString().toDouble();
        stepY = attrs.value("ystep").toString().toDouble();
        xDirection = attrs.value("xdirection").toString() == "true" ? true : false;
      } else {
        return 0;
      }
    } else if (xml.isEndElement()) {
      if (n == GeneratedMatrix::staticTypeTag) {
        break;
      } else {
        Debug::self()->log(QObject::tr("Error creating Generated Matrix from Kst file."), Debug::Warning);
        return 0;
      }
    }
    xml.readNext();
  }

  if (xml.hasError()) {
    return 0;
  }

  GeneratedMatrixPtr matrix = store->createObject<GeneratedMatrix>(tag);
  matrix->change(nX, nY, minX, minY, stepX, stepY, gradZMin, gradZMax, xDirection);

  matrix->writeLock();
  matrix->update(0);
  matrix->unlock();

  return matrix;
}


EditableMatrixFactory::EditableMatrixFactory()
: PrimitiveFactory() {
  registerFactory(EditableMatrix::staticTypeTag, this);
}


EditableMatrixFactory::~EditableMatrixFactory() {
}


PrimitivePtr EditableMatrixFactory::generatePrimitive(ObjectStore *store, QXmlStreamReader& xml) {
  ObjectTag tag;
  QByteArray data;

  Q_ASSERT(store);

  double minX, minY, nX, nY, stepX, stepY;

  while (!xml.atEnd()) {
      const QString n = xml.name().toString();
    if (xml.isStartElement()) {
      if (n == EditableMatrix::staticTypeTag) {
        QXmlStreamAttributes attrs = xml.attributes();
        tag = ObjectTag::fromString(attrs.value("tag").toString());
        minX = attrs.value("xmin").toString().toDouble();
        minY = attrs.value("ymin").toString().toDouble();
        nX = attrs.value("nx").toString().toDouble();
        nY = attrs.value("ny").toString().toDouble();
        stepX = attrs.value("xstep").toString().toDouble();
        stepY = attrs.value("ystep").toString().toDouble();
      } else if (n == "data") {

        QString qcs(xml.readElementText().toLatin1());
        QByteArray qbca = QByteArray::fromBase64(qcs.toLatin1());
        data = qUncompress(qbca);

      } else {
        return 0;
      }
    } else if (xml.isEndElement()) {
      if (n == EditableMatrix::staticTypeTag) {
        break;
      } else {
        Debug::self()->log(QObject::tr("Error creating Editable Matrix from Kst file."), Debug::Warning);
        return 0;
      }
    }
    xml.readNext();
  }

  if (xml.hasError()) {
    return 0;
  }

  EditableMatrixPtr matrix = store->createObject<EditableMatrix>(tag);
  matrix->change(data, nX, nY, minX, minY, stepX, stepY);

  matrix->writeLock();
  matrix->update(0);
  matrix->unlock();

  return matrix;
}



DataMatrixFactory::DataMatrixFactory()
: PrimitiveFactory() {
  registerFactory(DataMatrix::staticTypeTag, this);
}


DataMatrixFactory::~DataMatrixFactory() {
}


PrimitivePtr DataMatrixFactory::generatePrimitive(ObjectStore *store, QXmlStreamReader& xml) {
  ObjectTag tag;
  QByteArray data;

  Q_ASSERT(store);

  bool doAve, doSkip;
  int requestedXStart, requestedYStart, requestedXCount, requestedYCount, skip;
  QString provider, file, field;

  while (!xml.atEnd()) {
      const QString n = xml.name().toString();
    if (xml.isStartElement()) {
      if (n == DataMatrix::staticTypeTag) {
        QXmlStreamAttributes attrs = xml.attributes();
        tag = ObjectTag::fromString(attrs.value("tag").toString());
        provider = attrs.value("provider").toString();
        file = attrs.value("file").toString();
        field = attrs.value("field").toString();
        requestedXStart = attrs.value("reqxstart").toString().toInt();
        requestedYStart = attrs.value("reqystart").toString().toInt();
        requestedXCount = attrs.value("reqnx").toString().toInt();
        requestedYCount = attrs.value("reqny").toString().toInt();
        doAve = attrs.value("doave").toString() == "true" ? true : false;
        doSkip = attrs.value("doskip").toString() == "true" ? true : false;
        skip = attrs.value("skip").toString().toInt();
      } else {
        return 0;
      }
    } else if (xml.isEndElement()) {
      if (n == DataMatrix::staticTypeTag) {
        break;
      } else {
        Debug::self()->log(QObject::tr("Error creating Data Matrix from Kst file."), Debug::Warning);
        return 0;
      }
    }
    xml.readNext();
  }

  if (xml.hasError()) {
    return 0;
  }

  Q_ASSERT(store);
  DataSourcePtr dataSource = store->dataSourceList().findReusableFileName(file);

  if (!dataSource) {
    dataSource = DataSource::loadSource(store, file, QString());
  }

  if (!dataSource) {
    return 0; //Couldn't find a suitable datasource
  }

  DataMatrixPtr matrix = store->createObject<DataMatrix>(tag);
  matrix->change(dataSource, field, requestedXStart, requestedYStart, requestedXCount, requestedYCount, doAve, doSkip, skip);

  matrix->writeLock();
  matrix->update(0);
  matrix->unlock();

  return matrix;
}


}

// vim: ts=2 sw=2 et
