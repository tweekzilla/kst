/***************************************************************************
                   matrix.cpp: 2D matrix type for kst
                             -------------------
    begin                : July 2004
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   copyright : (C) 2007 The University of Toronto                        *
                     netterfield@astro.utoronto.ca
 *   copyright : (C) 2004  University of British Columbia                  *
 *                   dscott@phas.ubc.ca                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "matrix.h"

#include <math.h>
#include <QDebug>
#include <QXmlStreamWriter>

#include "debug.h"
#include "kst_i18n.h"
#include "math_kst.h"
#include "datacollection.h"
#include "objectstore.h"


// used for resizing; set to 1 for loop zeroing, 2 to use memset
#define ZERO_MEMORY 2

namespace Kst {

const QString Matrix::staticTypeString = I18N_NOOP("Matrix");

Matrix::Matrix(ObjectStore *store)
    : Primitive(store, 0L), _NS(0), _NRealS(0), _nX(1), _nY(0), _minX(0), _minY(0), _stepX(1), _stepY(1),
      _invertXHint(false), _invertYHint(false), _editable(false), _saveable(false), _z(0L), _zSize(0) {

  _initializeShortName();

  _scalars.clear();
  _strings.clear();
  _vectors.clear();

  setFlag(true);

  createScalars(store);

}


Matrix::~Matrix() {
  if (_z) {
    _vectors["z"]->setV(0L, 0);
    free(_z);
    _z = 0L;
  }
}

void Matrix::_initializeShortName() {
  _shortName = 'M'+QString::number(_mnum);
  if (_mnum>max_mnum)
    max_mnum = _mnum;
  _mnum++;
}


void Matrix::deleteDependents() {
  for (QHash<QString, ScalarPtr>::Iterator it = _scalars.begin(); it != _scalars.end(); ++it) {
    _store->removeObject(it.value());
  }
  for (QHash<QString, VectorPtr>::Iterator it = _vectors.begin(); it != _vectors.end(); ++it) {
    _store->removeObject(it.value());
  }
  Object::deleteDependents();
}


const QString& Matrix::typeString() const {
  return staticTypeString;
}


int Matrix::sampleCount() const {
  return _nX*_nY;
}


double Matrix::value(double x, double y, bool* ok) const {
  int x_index = (int)((x - _minX) / (double)_stepX);
  int y_index = (int)((y - _minY) / (double)_stepY);
  
  int index = zIndex(x_index, y_index);
  if ((index < 0) || !isfinite(_z[index]) || KST_ISNAN(_z[index])) {
    if (ok) (*ok) = false;
    return 0.0;
  }
  if (ok) (*ok) = true;
  return _z[index];

}


double Matrix::valueRaw(int x, int y, bool* ok) const {
  int index = zIndex(x,y);
  if ((index < 0) || !isfinite(_z[index]) || KST_ISNAN(_z[index])) {
    if (ok) {
      (*ok) = false;
    }
    return 0.0;
  }
  if (ok) {
    (*ok) = true;
  }
  return _z[index];
}


int Matrix::zIndex(int x, int y) const {
  if (x >= _nX || x < 0 || y >= _nY || y < 0) {
    return -1;
  }
  int index = x * _nY + y;
  if (index >= _zSize || index < 0 ) {
    return -1;
  }
  return index;
}


bool Matrix::setValue(double x, double y, double z) {
  int x_index = (int)floor((x - _minX) / (double)_stepX);
  int y_index = (int)floor((y - _minY) / (double)_stepY);
  return setValueRaw(x_index, y_index, z);
}


bool Matrix::setValueRaw(int x, int y, double z) {
  int index = zIndex(x,y);
  if (index < 0) {
    return false;
  }
  _z[index] = z;
  return true;
}

double Matrix::minValue() const {
  return _scalars["min"]->value();
}


double Matrix::maxValue() const {
  return _scalars["max"]->value();
}

double Matrix::minValueNoSpike() const {
  // FIXME: it is expensive to calcNoSpikeRange
  // so we have chosen here to only call it expicitly
  // and no attempt is made to check if it is still up to date...
  // It would be better to have these calls call
  // calcNoSpikeRange iff the values were obsolete.
  return _minNoSpike;
}

double Matrix::maxValueNoSpike() const {
  // FIXME: it is expensive to calcNoSpikeRange
  // so we have chosen here to only call it expicitly
  // and no attempt is made to check if it is still up to date...
  // It would be better to have these calls call
  // calcNoSpikeRange iff the values were obsolete.

  return _maxNoSpike;
}

void Matrix::calcNoSpikeRange(double per) {
  double *min_list, *max_list, min_of_max, max_of_min;
  int n_list;
  int max_n = 50000; // the most samples we will look at...
  double n_skip;
  double x=0;
  int n_notnan;

  int i,j, k;

  // count number of points which aren't nans.
  for (i=n_notnan=0; i<_NS; i++) {
    if (!KST_ISNAN(_z[i])) {
      n_notnan++;
    }
  }

  if (n_notnan==0) {
    _minNoSpike = 0;
    _maxNoSpike = 0;

    return;
  }

  if (per < 0) {
    per = 0;
  }
  per *= (double)n_notnan/(double)_NS;
  max_n *= int((double)_NS/(double)n_notnan);

  n_skip = (double)_NS/max_n;
  if (n_skip<1.0) n_skip = 1.0;

  n_list = int(double(_NS)*per/n_skip);

  min_list = (double *)malloc(n_list * sizeof(double));
  max_list = (double *)malloc(n_list * sizeof(double));


  // prefill the list
  for (i=0; i<n_list; i++) {
    min_list[i] = 1E+300;
    max_list[i] = -1E+300;
  }
  min_of_max = -1E+300;
  max_of_min = 1E+300;

  i = n_list;
  for (j=0; j<_NS; j=int(i*n_skip), i++) {
    if (_z[j] < max_of_min) { // member for the min list
      // replace max of min with the new value
      for (k=0; k<n_list; k++) {
        if (min_list[k]==max_of_min) {
          x = min_list[k] = _z[j];
          break;
        }
      }
      max_of_min = x;
      // find the new max_of_min
      for (k=0; k<n_list; k++) {
        if (min_list[k] > max_of_min) {
          max_of_min = min_list[k];
        }
      }
    }
    if (_z[j] > min_of_max) { // member for the max list
      //printf("******** z: %g  min_of_max: %g\n", _z[j], min_of_max);
      // replace min of max with the new value
      for (k=0; k<n_list; k++) {
        if (max_list[k]==min_of_max) {
          x = max_list[k] = _z[j];
          break;
        }
      }
      // find the new min_of_max
      min_of_max = x;
      for (k=0; k<n_list; k++) {
        if (max_list[k] < min_of_max) {
          min_of_max = max_list[k];
        }
      }
    }
  }

  // FIXME: this needs a z spike insensitive algorithm...
  _minNoSpike = max_of_min;
  _maxNoSpike = min_of_max;

  free(min_list);
  free(max_list);
}

double Matrix::meanValue() const {
  return _scalars["mean"]->value();
}

double Matrix::minValuePositive() const {
  return _scalars["minpos"]->value();
}

int Matrix::numNew() const {
  return _numNew;
}


void Matrix::resetNumNew() {
  _numNew = 0;
}


void Matrix::zero() {
  for (int i = 0; i < _zSize; i++) {
    _z[i] = 0.0;
  }
  updateScalars();
}


void Matrix::blank() {
  for (int i = 0; i < _zSize; ++i) {
    _z[i] = NOPOINT;
  }
  updateScalars();
}


int Matrix::getUsage() const {
  int scalarUsage = 0;
  for (QHash<QString, ScalarPtr>::ConstIterator it = _scalars.begin(); it != _scalars.end(); ++it) {
    scalarUsage += it.value()->getUsage() - 1;
  }
  return Object::getUsage() + scalarUsage;
}


void Matrix::internalUpdate() {
  // calculate stats
  _NS = _nX * _nY;

  if (_zSize > 0) {
    double min = NAN;
    double max = NAN;
    double minpos = NAN;
    double sum = 0.0, sumsquared = 0.0;
    bool initialized = false;

    _NRealS = 0;

    for (int i = 0; i < _zSize; i++) {
      if (isfinite(_z[i]) && !KST_ISNAN(_z[i])) {
        if (!initialized) {
          min = _z[i];
          max = _z[i];
          minpos = (_z[0] > 0) ? _z[0] : 1.0E300;
          initialized = true;
          _NRealS++;
        } else {
          if (min > _z[i]) {
            min = _z[i];
          }
          if (max < _z[i]) {
            max = _z[i];
          }
          if (minpos > _z[i] && _z[i] > 0) {
            minpos = _z[i];
          }
          sum += _z[i];
          sumsquared += _z[i] * _z[i];

          _NRealS++;
        }
      }
    }
    _scalars["sum"]->setValue(sum);
    _scalars["sumsquared"]->setValue(sumsquared);
    _scalars["max"]->setValue(max);
    _scalars["min"]->setValue(min);
    _scalars["minpos"]->setValue(minpos);

    updateScalars();
  }
}

void Matrix::setXLabelInfo(const LabelInfo &label_info) {
  _xLabelInfo = label_info;
}

void Matrix::setYLabelInfo(const LabelInfo &label_info) {
  _yLabelInfo = label_info;
}

void Matrix::setTitleInfo(const LabelInfo &label_info) {
  _titleInfo = label_info;
}

LabelInfo Matrix::xLabelInfo() const {
  return _xLabelInfo;
}

LabelInfo Matrix::yLabelInfo() const {
  return _yLabelInfo;
}


LabelInfo Matrix::titleInfo() const {
  return _titleInfo;
}

bool Matrix::editable() const {
  return _editable;
}


void Matrix::setEditable(bool editable) {
  _editable = editable;
}


void Matrix::createScalars(ObjectStore *store) {
  Q_ASSERT(store);
  ScalarPtr sp;
  VectorPtr vp;

  _scalars.insert("max", sp=store->createObject<Scalar>());
  sp->setProvider(this);
  sp->setSlaveName("Max");

  _scalars.insert("min", sp=store->createObject<Scalar>());
  sp->setProvider(this);
  sp->setSlaveName("Min");

  _scalars.insert("mean", sp=store->createObject<Scalar>());
  sp->setProvider(this);
  sp->setSlaveName("Mean");

  _scalars.insert("sigma", sp=store->createObject<Scalar>());
  sp->setProvider(this);
  sp->setSlaveName("Sigma");

  _scalars.insert("rms", sp=store->createObject<Scalar>());
  sp->setProvider(this);
  sp->setSlaveName("Rms");

  _scalars.insert("ns", sp=store->createObject<Scalar>());
  sp->setProvider(this);
  sp->setSlaveName("NS");

  _scalars.insert("sum", sp=store->createObject<Scalar>());
  sp->setProvider(this);
  sp->setSlaveName("Sum");

  _scalars.insert("sumsquared", sp=store->createObject<Scalar>());
  sp->setProvider(this);
  sp->setSlaveName("SumSquared");

  _scalars.insert("minpos", sp=store->createObject<Scalar>());
  sp->setProvider(this);
  sp->setSlaveName("MinPos");

  _vectors.insert("z", vp = store->createObject<Vector>());
  vp->setProvider(this);
  vp->setSlaveName("Z");

}


void Matrix::updateScalars() {
  _scalars["ns"]->setValue(_NS);
  if (_NRealS >= 2) {
    _scalars["mean"]->setValue(_scalars["sum"]->value()/double(_NRealS));
    _scalars["sigma"]->setValue( sqrt(
        (_scalars["sumsquared"]->value() - _scalars["sum"]->value()*_scalars["sum"]->value()/double(_NRealS))/ double(_NRealS-1) ) );
    _scalars["rms"]->setValue(sqrt(_scalars["sumsquared"]->value()/double(_NRealS)));
  } else {
    _scalars["sigma"]->setValue(_scalars["max"]->value() - _scalars["min"]->value());
    _scalars["rms"]->setValue(sqrt(_scalars["sumsquared"]->value()));
    _scalars["mean"]->setValue(0);
  }
}


bool Matrix::resizeZ(int sz, bool reinit) {
//   qDebug() << "resizing to: " << sz << endl;
  if (sz >= 1) {
    if (!kstrealloc(_z, sz*sizeof(double))) {
      qCritical() << "Matrix resize failed";
      return false;
    }
    _vectors["z"]->setV(_z, sz);
#ifdef ZERO_MEMORY
    if (reinit && _zSize < sz) {
#if ZERO_MEMORY == 2
      memset(&_z[_zSize], 0, (sz - _zSize)*sizeof(double));

#else
      for (int i = _zSize; i < sz; i++) {
        _z[i] = 0.0;
      }
#endif
    }
#else
    // TODO: Is aborting all we can do?
    fatalError("Not enough memory for matrix data");
    return false;
#endif
    _zSize = sz;
    updateScalars();
  }
  return true;
}


#if 0
bool Matrix::resize(int xSize, int ySize, bool reinit) {
  int oldNX = _nX;
  int oldNY = _nY;
  _nX = xSize;
  _nY = ySize;
  if (resizeZ(xSize*ySize, reinit)) {
    return true;
  } else {
    _nX = oldNX;
    _nY = oldNY;
    return false;
  }
}
#endif


// Resize the matrix to xSize x ySize, maintaining the values in the current
// positions. If reinit is set, new entries will be initialized to 0.
// Otherwise, they will not be set.  The behavior in that case is undefined.
bool Matrix::resize(int xSize, int ySize, bool reinit) {
  if (xSize <= 0 || ySize <= 0) {
    return false;
  }

  // NOTE: _zSize is assumed to correctly represent the state of _z, while _nX
  // and _nY are the desired (but not necessarily actual) current size of the
  // matrix
  bool valid = (_zSize == _nX * _nY); // is the current matrix properly initialized?

  int sz = xSize * ySize;
  if (sz > _zSize) {
    // array is getting bigger, so resize before moving
    if (!kstrealloc(_z, sz*sizeof(double))) {
      qCritical() << "Matrix resize failed";
      return false;
    }
    _vectors["z"]->setV(_z, sz);
  }

  if (valid && ySize != _nY && _nY > 0) {
    // move old values to new spots
    int source = 0;
    int target = 0;
    for (int row=1; row < qMin(xSize, _nX); ++row) {
      source += _nY;
      target += ySize;
      memmove(_z + target, _z + source, qMin(ySize, _nY)*sizeof(double));
      if (reinit && ySize > _nY) {
        // initialize memory in new column(s) of previous row vacated by memmove
        memset(_z + source, 0, (ySize - _nY)*sizeof(double));
      }
    }
  }

  if (sz < _zSize) {
    // array is getting smaller, so resize after moving
    if (!kstrealloc(_z, sz*sizeof(double))) {
      qCritical() << "Matrix resize failed";
      return false;
    }
    _vectors["z"]->setV(_z, sz);
  }

  if (reinit && _zSize < sz) {
    // initialize new memory after old values
    for (int row=0; row < qMin(xSize, _nX); ++row) {
      for (int col = qMin(ySize,_nY); col<ySize; col++) {
        _z[row*ySize+col] = 0;
      }
    }
    for (int row = qMin(xSize, _nX); row < xSize; row++) {
      for (int col = 0; col < ySize; col++) {
        _z[row*ySize+col] = 0;
      }
    }
  }

  _nX = xSize;
  _nY = ySize;
  _NS = _nX * _nY;
  _zSize = sz;

  updateScalars();

  return true;
}


void Matrix::save(QXmlStreamWriter &s) {
  Q_UNUSED(s)
  // no saving
}


bool Matrix::saveable() const {
  return _saveable;
}


void Matrix::change(uint nX, uint nY, double minX, double minY, double stepX, double stepY) {
  _nX = nX;
  _nY = nY;
  _stepX = stepX;
  _stepY = stepY;
  _minX = minX;
  _minY = minY;
  resizeZ(nX*nY, true);
}


void Matrix::change(QByteArray &data, uint nX, uint nY, double minX, double minY, double stepX, double stepY) {
  _nX = nX;
  _nY = nY;
  _minX = minX;
  _minY = minY;
  _stepX = stepX;
  _stepY = stepY;

  _saveable = true;
  resizeZ(nX*nY, true);

  QDataStream qds(&data, QIODevice::ReadOnly);
  uint i;
  // fill in the raw array with the data
  for (i = 0; i < nX*nY && !qds.atEnd(); i++) {
    qds >> _z[i];  // stored in the same order as it was saved
  } 
  if (i < nX*nY) {
    Debug::self()->log(i18n("Saved matrix contains less data than it claims."), Debug::Warning);
    resizeZ(i, false);
  }
  internalUpdate();
}

QString Matrix::descriptionTip() const {
  return i18n("Matrix: %1\n %2 x %3").arg(Name()).arg(_nX).arg(_nY);
}

QString Matrix::sizeString() const {
  return QString("%1x%2").arg(_nX).arg(_nY);
}

ObjectList<Primitive> Matrix::outputPrimitives() const {
  PrimitiveList primitive_list;
  int n;

  n = _scalars.count();
  for (int i = 0; i< n; i++) {
      primitive_list.append(kst_cast<Primitive>(_scalars.values().at(i)));
  }

  n = _strings.count();
  for (int i = 0; i< n; i++) {
      primitive_list.append(kst_cast<Primitive>(_strings.values().at(i)));
  }

  n = _vectors.count();
  for (int i = 0; i< n; i++) {
    VectorPtr V = _vectors.values().at(i);
    primitive_list.append(kst_cast<Primitive>(V));
    primitive_list.append(V->outputPrimitives());
  }

  return primitive_list;
}

PrimitiveMap Matrix::metas() const
{
  PrimitiveMap meta;
  for (QHash<QString, StringPtr>::ConstIterator it = _strings.begin(); it != _strings.end(); ++it) {
    meta[it.key()] = it.value();
  }
  for (QHash<QString, ScalarPtr>::ConstIterator it = _scalars.begin(); it != _scalars.end(); ++it) {
    meta[it.key()] = it.value();
  }
  for (QHash<QString, VectorPtr>::ConstIterator it = _vectors.begin(); it != _vectors.end(); ++it) {
    meta[it.key()] = it.value();
  }
  return meta;
}

QByteArray Matrix::getBinaryArray() const {
    readLock();
    QByteArray ret;
    QDataStream ds(&ret, QIODevice::WriteOnly);
    ds<<(qint32)_nX<<(qint32)_nY<<_minX<<_minY<<_stepX<<_stepY; //fixme: this makes it not compatible w/ change(...)

    uint i;
    uint n = _nX*_nY;
    // fill in the raw array with the data
    for (i = 0; i < n; i++) {
      ds << _z[i];
    }
    unlock();
    return ret;
}

}
// vim: ts=2 sw=2 et
