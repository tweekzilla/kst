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

#include <QDebug>
#include <QPainterPath>

#include "plotitem.h"

#include "plotrenderitem.h"

#include "kstsvector.h"
#include "kstvcurve.h"
#include "kstdatacollection.h"
#include "kstdataobjectcollection.h"
#include "vectorcurverenderitem.h"

#include <QDebug>

static qreal MARGIN_WIDTH = 20.0;
static qreal MARGIN_HEIGHT = 20.0;

namespace Kst {

PlotItem::PlotItem(View *parent)
  : ViewItem(parent), _marginWidth(0), _marginHeight(0) {

  // FIXME fake data for testing rendering
  KstVectorPtr xTest = new KstSVector(0.0, 100.0, 10000, KstObjectTag::fromString("X vector"));
  xTest->setLabel("a nice x label");
  KstVectorPtr yTest = new KstSVector(0.0, 100.0, 10000, KstObjectTag::fromString("Y vector"));
  yTest->setLabel("a nice y label");

  KstVectorPtr yTest2 = new KstSVector(-100.0, 100.0, 10000, KstObjectTag::fromString("Y vector 2"));
  yTest2->setLabel("another nice y label");

  KstVectorPtr errorX = new KstSVector(0.0, 0.0, 0, KstObjectTag::fromString("X error"));
  KstVectorPtr errorY = new KstSVector(0.0, 0.0, 0, KstObjectTag::fromString("y error"));

  KstVCurvePtr renderTest = new KstVCurve(QString("rendertest"), xTest, yTest, errorX, errorY, errorX, errorY, QColor(Qt::red));
  renderTest->writeLock();
  renderTest->update(0);
  renderTest->unlock();

  KstVCurvePtr renderTest2 = new KstVCurve(QString("rendertest2"), xTest, yTest2, errorX, errorY, errorX, errorY, QColor(Qt::blue));
  renderTest2->writeLock();
  renderTest2->update(0);
  renderTest2->unlock();

  KstRelationList relationList;
  relationList.append(kst_cast<KstRelation>(renderTest));
  relationList.append(kst_cast<KstRelation>(renderTest2));

  VectorCurveRenderItem *test = new VectorCurveRenderItem("cartesiantest", this);
  test->setRelationList(relationList);

  _renderers.append(test);
}


PlotItem::~PlotItem() {
}


void CreatePlotCommand::createItem() {
  _item = new PlotItem(_view);
  _view->setCursor(Qt::CrossCursor);

  CreateCommand::createItem();
}


void PlotItem::paint(QPainter *painter) {
  ViewItem::paint(painter);

  painter->save();
  painter->translate(QPointF(rect().x(), rect().y()));

  //Calculate and adjust the margins based on the bounds...
  QSizeF margins;
  margins = margins.expandedTo(calculateLeftLabelBound(painter));
  margins = margins.expandedTo(calculateBottomLabelBound(painter));
  margins = margins.expandedTo(calculateRightLabelBound(painter));
  margins = margins.expandedTo(calculateTopLabelBound(painter));

//  qDebug() << "setting margin width" << margins.width() << endl;
  setMarginWidth(margins.width());

//  qDebug() << "setting margin height" << margins.height() << endl;
  setMarginHeight(margins.height());

//  qDebug() << "=============> leftLabel:" << leftLabel() << endl;
  paintLeftLabel(painter);
//  qDebug() << "=============> bottomLabel:" << bottomLabel() << endl;
  paintBottomLabel(painter);
//  qDebug() << "=============> rightLabel:" << rightLabel() << endl;
  paintRightLabel(painter);
//  qDebug() << "=============> topLabel:" << topLabel() << endl;
  paintTopLabel(painter);

  painter->restore();
}


qreal PlotItem::marginWidth() const {
  qreal m = qMax(MARGIN_WIDTH, _marginWidth);

  //No more than 1/4 the width of the plot
  if (width() < m * 4)
    return width() / 4;

  return m;
}


void PlotItem::setMarginWidth(qreal marginWidth) {
  qreal before = this->marginWidth();
  _marginWidth = marginWidth;
  if (before != this->marginWidth())
    emit geometryChanged();
}


qreal PlotItem::marginHeight() const {
  qreal m = qMax(MARGIN_HEIGHT, _marginHeight);

  //No more than 1/4 the height of the plot
  if (height() < m * 4)
    return height() / 4;

  return m;
}


void PlotItem::setMarginHeight(qreal marginHeight) {
  qreal before = this->marginHeight();
  _marginHeight = marginHeight;
  if (before != this->marginHeight())
    emit geometryChanged();
}


QString PlotItem::leftLabel() const {
  foreach (PlotRenderItem *renderer, _renderers) {
    if (!renderer->leftLabel().isEmpty())
      return renderer->leftLabel();
  }
  return QString();
}


QString PlotItem::bottomLabel() const {
  foreach (PlotRenderItem *renderer, _renderers) {
    if (!renderer->bottomLabel().isEmpty())
      return renderer->bottomLabel();
  }
  return QString();
}


QString PlotItem::rightLabel() const {
  foreach (PlotRenderItem *renderer, _renderers) {
    if (!renderer->rightLabel().isEmpty())
      return renderer->rightLabel();
  }
  return QString();
}


QString PlotItem::topLabel() const {
  foreach (PlotRenderItem *renderer, _renderers) {
    if (!renderer->topLabel().isEmpty())
      return renderer->topLabel();
  }
  return QString();
}


QRectF PlotItem::horizontalLabelRect() const {
  return QRectF(0.0, 0.0, width() - 2.0 * marginWidth(), marginHeight());
}


QRectF PlotItem::verticalLabelRect() const {
  return QRectF(0.0, 0.0, marginWidth(), height() - 2.0 * marginHeight());
}


void PlotItem::paintLeftLabel(QPainter *painter) {
  painter->save();
  QTransform t;
  t.rotate(90.0);
  painter->rotate(-90.0);

  QRectF leftLabelRect = verticalLabelRect();
  leftLabelRect.moveTopLeft(QPointF(0.0, marginHeight()));
  painter->drawText(t.mapRect(leftLabelRect), Qt::TextWordWrap | Qt::AlignCenter, leftLabel());
  painter->restore();
}


QSizeF PlotItem::calculateLeftLabelBound(QPainter *painter) {
  painter->save();
  QTransform t;
  t.rotate(90.0);
  painter->rotate(-90.0);

  QRectF leftLabelBound = painter->boundingRect(t.mapRect(verticalLabelRect()),
                                                Qt::TextWordWrap | Qt::AlignCenter, leftLabel());
  painter->restore();

  QSizeF margins;
  margins.setWidth(leftLabelBound.height());
  return margins;
}


void PlotItem::paintBottomLabel(QPainter *painter) {
  painter->save();
  QRectF bottomLabelRect = horizontalLabelRect();
  bottomLabelRect.moveTopLeft(QPointF(marginWidth(), height() - marginHeight()));
  painter->drawText(bottomLabelRect, Qt::TextWordWrap | Qt::AlignCenter, bottomLabel());
  painter->restore();
}


QSizeF PlotItem::calculateBottomLabelBound(QPainter *painter) {
  QRectF bottomLabelBound = painter->boundingRect(horizontalLabelRect(),
                                                  Qt::TextWordWrap | Qt::AlignCenter, bottomLabel());

  QSizeF margins;
  margins.setHeight(bottomLabelBound.height());
  return margins;
}


void PlotItem::paintRightLabel(QPainter *painter) {
  painter->save();
  painter->translate(width() - marginWidth(), 0.0);
  QTransform t;
  t.rotate(-90.0);
  painter->rotate(90.0);

  //same as left but painter is translated
  QRectF rightLabelRect = verticalLabelRect();
  rightLabelRect.moveTopLeft(QPointF(0.0, marginHeight()));
  painter->drawText(t.mapRect(rightLabelRect), Qt::TextWordWrap | Qt::AlignCenter, rightLabel());
  painter->restore();
}


QSizeF PlotItem::calculateRightLabelBound(QPainter *painter) {
  painter->save();
  QTransform t;
  t.rotate(-90.0);
  painter->rotate(90.0);
  QRectF rightLabelBound = painter->boundingRect(t.mapRect(verticalLabelRect()),
                                                 Qt::TextWordWrap | Qt::AlignCenter, rightLabel());
  painter->restore();

  QSizeF margins;
  margins.setWidth(rightLabelBound.height());
  return margins;
}


void PlotItem::paintTopLabel(QPainter *painter) {
  painter->save();
  QRectF topLabelRect = horizontalLabelRect();
  topLabelRect.moveTopLeft(QPointF(marginWidth(), 0.0));
  painter->drawText(topLabelRect, Qt::TextWordWrap | Qt::AlignCenter, bottomLabel());
  painter->restore();
}


QSizeF PlotItem::calculateTopLabelBound(QPainter *painter) {
  QRectF topLabelBound = painter->boundingRect(horizontalLabelRect(),
                                               Qt::TextWordWrap | Qt::AlignCenter, topLabel());

  QSizeF margins;
  margins.setHeight(topLabelBound.height());
  return margins;
}

}

// vim: ts=2 sw=2 et
