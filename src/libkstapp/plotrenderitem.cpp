/***************************************************************************
 *                                                                         *
 *   copyright : (C) 2007 Theodore Kisner <tsk@humanityforward.org>        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "plotrenderitem.h"

#include "plotitem.h"
#include "viewitemzorder.h"
#include "plotitemmanager.h"
#include "application.h"
#include "objectstore.h"

#include <QTime>
#include <QMenu>
#include <QStatusBar>
#include <QMainWindow>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QKeyEvent>

// #define CURVE_DRAWING_TIME

namespace Kst {

PlotRenderItem::PlotRenderItem(PlotItem *parentItem)
  : ViewItem(parentItem->parentView()),
  _xAxisZoomMode(Auto),
  _yAxisZoomMode(AutoBorder) {

  setName(tr("Plot Render"));
  setZValue(PLOTRENDER_ZVALUE);
  setParent(parentItem);
  setHasStaticGeometry(true);
  setAllowedGripModes(0);
  setAllowedGrips(0);

  connect(parentItem, SIGNAL(geometryChanged()),
          this, SLOT(updateGeometry()));
  connect(parentItem, SIGNAL(marginsChanged()),
          this, SLOT(updateGeometry()));
  connect(parentItem, SIGNAL(updatePlotRect()),
          this, SLOT(updateGeometry()));
  connect(parentItem->parentView(), SIGNAL(viewModeChanged(View::ViewMode)),
          this, SLOT(updateViewMode()));

  connect(this, SIGNAL(projectionRectChanged()),
          parentItem, SLOT(calculateProjectionRect()));

  updateGeometry(); //the initial rect
  updateViewMode(); //the initial view
  createActions();
}


PlotRenderItem::~PlotRenderItem() {
}


PlotItem *PlotRenderItem::plotItem() const {
  return qobject_cast<PlotItem*>(qgraphicsitem_cast<ViewItem*>(parentItem()));
}


PlotRenderItem::RenderType PlotRenderItem::type() {
  return _type;
}


void PlotRenderItem::setType(PlotRenderItem::RenderType type) {
  _type = type;
}


bool PlotRenderItem::isTiedZoom() const {
  return plotItem()->isTiedZoom();
}


void PlotRenderItem::setTiedZoom(bool tiedZoom) {
  return plotItem()->setTiedZoom(tiedZoom);
}


PlotRenderItem::ZoomMode PlotRenderItem::xAxisZoomMode() const {
  return _xAxisZoomMode;
}


void PlotRenderItem::setXAxisZoomMode(ZoomMode mode) {
  _xAxisZoomMode = mode;
}


PlotRenderItem::ZoomMode PlotRenderItem::yAxisZoomMode() const {
  return _yAxisZoomMode;
}


void PlotRenderItem::setYAxisZoomMode(ZoomMode mode) {
  _yAxisZoomMode = mode;
}


QRectF PlotRenderItem::plotRect() const {
  QRectF plotRect = rect();
  plotRect = plotRect.normalized();
  plotRect.moveTopLeft(QPointF(0.0, 0.0));
  return plotRect;
}


QRectF PlotRenderItem::projectionRect() const {
    return _projectionRect;
}


void PlotRenderItem::setProjectionRect(const QRectF &rect) {
  if (_projectionRect == rect || rect.isEmpty() || !rect.isValid())
    return;

  qDebug() << "=== setProjectionRect() ======================>\n"
            << "computedProjectionRect" << computedProjectionRect() << "\n"
            << "before:" << _projectionRect << "\n"
            << "after:" << rect << endl;

  _projectionRect = rect;
  emit projectionRectChanged();
}


RelationList PlotRenderItem::relationList() const {
  return _relationList;
}


void PlotRenderItem::addRelation(RelationPtr relation) {
  _relationList.append(relation);
  zoomMaximum();
}


void PlotRenderItem::removeRelation(RelationPtr relation) {
  _relationList.removeAll(relation);
  zoomMaximum();
}


void PlotRenderItem::clearRelations() {
  _relationList.clear();
  zoomMaximum();
}


void PlotRenderItem::save(QXmlStreamWriter &xml) {
  Q_UNUSED(xml);
}


void PlotRenderItem::saveInPlot(QXmlStreamWriter &xml) {
  xml.writeAttribute("name", name());
  xml.writeAttribute("type", QVariant(_type).toString());
  xml.writeAttribute("xzoommode", QVariant(_xAxisZoomMode).toString());
  xml.writeAttribute("yzoommode", QVariant(_yAxisZoomMode).toString());
  xml.writeStartElement("rect");
  xml.writeAttribute("x", QVariant(projectionRect().x()).toString());
  xml.writeAttribute("y", QVariant(projectionRect().y()).toString());
  xml.writeAttribute("width", QVariant(projectionRect().width()).toString());
  xml.writeAttribute("height", QVariant(projectionRect().height()).toString());
  xml.writeEndElement();
  foreach (RelationPtr relation, relationList()) {
    xml.writeStartElement("relation");
    xml.writeAttribute("tag", relation->tag().tagString());
    xml.writeEndElement();
  }
}


bool PlotRenderItem::configureFromXml(QXmlStreamReader &xml, ObjectStore *store) {
  bool validTag = true;
  double x = 0, y = 0, w = 10, h = 10;

  QString primaryTag = xml.name().toString();
  QXmlStreamAttributes attrs = xml.attributes();
  QStringRef av;
  av = attrs.value("type");
  if (!av.isNull()) {
    setType((RenderType)av.toString().toInt());
  }
  av = attrs.value("xzoommode");
  if (!av.isNull()) {
    setXAxisZoomMode((ZoomMode)av.toString().toInt());
  }
  av = attrs.value("yzoommode");
  if (!av.isNull()) {
    setYAxisZoomMode((ZoomMode)av.toString().toInt());
  }

  QString expectedEnd;
  while (!(xml.isEndElement() && (xml.name().toString() == primaryTag))) {
    if (xml.isStartElement() && xml.name().toString() == "rect") {
      expectedEnd = xml.name().toString();
      attrs = xml.attributes();
      av = attrs.value("width");
      if (!av.isNull()) {
        w = av.toString().toDouble();
      }
      av = attrs.value("height");
      if (!av.isNull()) {
        h = av.toString().toDouble();
      }
      av = attrs.value("x");
      if (!av.isNull()) {
        x = av.toString().toDouble();
      }
      av = attrs.value("y");
      if (!av.isNull()) {
        y = av.toString().toDouble();
      }
    } else if (xml.isStartElement() && xml.name().toString() == "relation") {
      expectedEnd = xml.name().toString();
      attrs = xml.attributes();
      ObjectTag tag = ObjectTag::fromString(attrs.value("tag").toString());
      qDebug(tag.tagString());
      RelationPtr relation = kst_cast<Relation>(store->retrieveObject(tag));
      if (relation) {
        addRelation(relation);
      }
    } else if (xml.isEndElement()) {
      if (xml.name().toString() != expectedEnd) {
        validTag = false;
        break;
      }
    }
    xml.readNext();
  }
  setProjectionRect(QRectF(QPointF(x, y), QSizeF(w, h)));

  return validTag;
}


void PlotRenderItem::paint(QPainter *painter) {
  painter->setRenderHint(QPainter::Antialiasing, false);

#ifdef CURVE_DRAWING_TIME
  QTime time;
  time.start();
#endif

  painter->save();

  if (plotItem()->xAxisReversed()) {
    painter->scale(-1, 1);
    painter->translate(-1.0 * rect().right() - rect().left(), 0);
  }
  if (plotItem()->yAxisReversed()) {
    painter->scale(1, -1);
    painter->translate(0, -1.0 * rect().bottom() - rect().top());
  }
  painter->setClipRect(rect());
  paintRelations(painter);

  if (_selectionRect.isValid()) {
    painter->setPen(QPen(QBrush(Qt::black), 1.0, Qt::DotLine));
    painter->drawRect(_selectionRect.rect());
  }
  painter->restore();

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->fillPath(checkBox(), Qt::white);
  if (isHovering()) {
    QRectF check = checkBox().controlPointRect();
    check.setSize(QSizeF(check.width() / 1.8, check.height() / 1.8));
    check.moveCenter(checkBox().controlPointRect().center());
    QPainterPath p;
    p.addEllipse(check);
    painter->fillPath(p, Qt::black);
  }
  if (isTiedZoom()) {
    painter->save();
    painter->setOpacity(0.5);
    painter->fillPath(checkBox(), Qt::black);
    painter->restore();
  }
  painter->drawPath(checkBox());
  painter->restore();

#ifdef CURVE_DRAWING_TIME
  int elapsed = time.elapsed();
  qDebug()<<"curve drawing took" << elapsed << "to render.";
#endif
}


QString PlotRenderItem::leftLabel() const {
  foreach (RelationPtr relation, relationList()) {
    if (!relation->yLabel().isEmpty())
      return relation->yLabel();
  }
  return QString();
}


QString PlotRenderItem::bottomLabel() const {
  foreach (RelationPtr relation, relationList()) {
    if (!relation->xLabel().isEmpty())
      return relation->xLabel();
  }
  return QString();
}


QString PlotRenderItem::rightLabel() const {
  //FIXME much less than ideal
  QString left = leftLabel();
  foreach (RelationPtr relation, relationList()) {
    if (!relation->yLabel().isEmpty() && relation->yLabel() != left)
      return relation->yLabel();
  }
  return QString();
}


QString PlotRenderItem::topLabel() const {
  //FIXME much less than ideal
  QString bottom = bottomLabel();
  foreach (RelationPtr relation, relationList()) {
    if (!relation->xLabel().isEmpty() && relation->xLabel() != bottom)
      return relation->xLabel();
  }
  return QString();
}


void PlotRenderItem::createActions() {
  _zoomMaximum = new QAction(tr("Zoom Maximum"), this);
  _zoomMaximum->setShortcut(Qt::Key_M);
  registerShortcut(_zoomMaximum);
  connect(_zoomMaximum, SIGNAL(triggered()), this, SLOT(zoomMaximum()));

  _zoomMaxSpikeInsensitive = new QAction(tr("Zoom Max Spike Insensitive"), this);
  _zoomMaxSpikeInsensitive->setShortcut(Qt::Key_S);
  registerShortcut(_zoomMaxSpikeInsensitive);
  connect(_zoomMaxSpikeInsensitive, SIGNAL(triggered()), this, SLOT(zoomMaxSpikeInsensitive()));

  _zoomYMeanCentered = new QAction(tr("Y-Zoom Mean-centered"), this);
  _zoomYMeanCentered->setShortcut(Qt::Key_A);
  registerShortcut(_zoomYMeanCentered);
  connect(_zoomYMeanCentered, SIGNAL(triggered()), this, SLOT(zoomYMeanCentered()));

  _zoomXMaximum = new QAction(tr("X-Zoom Maximum"), this);
  _zoomXMaximum->setShortcut(Qt::CTRL+Qt::Key_M);
  registerShortcut(_zoomXMaximum);
  connect(_zoomXMaximum, SIGNAL(triggered()), this, SLOT(zoomXMaximum()));

  _zoomXRight = new QAction(tr("X-Zoom Right"), this);
  _zoomXRight->setShortcut(Qt::Key_Right);
  registerShortcut(_zoomXRight);
  connect(_zoomXRight, SIGNAL(triggered()), this, SLOT(zoomXRight()));

  _zoomXLeft= new QAction(tr("X-Zoom Left"), this);
  _zoomXLeft->setShortcut(Qt::Key_Left);
  registerShortcut(_zoomXLeft);
  connect(_zoomXLeft, SIGNAL(triggered()), this, SLOT(zoomXLeft()));

  _zoomXOut = new QAction(tr("X-Zoom Out"), this);
  _zoomXOut->setShortcut(Qt::SHIFT+Qt::Key_Right);
  registerShortcut(_zoomXOut);
  connect(_zoomXOut, SIGNAL(triggered()), this, SLOT(zoomXOut()));

  _zoomXIn = new QAction(tr("X-Zoom In"), this);
  _zoomXIn->setShortcut(Qt::SHIFT+Qt::Key_Left);
  registerShortcut(_zoomXIn);
  connect(_zoomXIn, SIGNAL(triggered()), this, SLOT(zoomXIn()));

  _zoomNormalizeXtoY = new QAction(tr("Normalize X Axis to Y Axis"), this);
  _zoomNormalizeXtoY->setShortcut(Qt::Key_N);
  registerShortcut(_zoomNormalizeXtoY);
  connect(_zoomNormalizeXtoY, SIGNAL(triggered()), this, SLOT(zoomNormalizeXtoY()));

  _zoomLogX = new QAction(tr("Log X Axis"), this);
  _zoomLogX->setShortcut(Qt::Key_G);
  _zoomLogX->setCheckable(true);
  registerShortcut(_zoomLogX);
  connect(_zoomLogX, SIGNAL(triggered()), this, SLOT(zoomLogX()));

  _zoomYLocalMaximum = new QAction(tr("Y-Zoom Local Maximum"), this);
  _zoomYLocalMaximum->setShortcut(Qt::SHIFT+Qt::Key_L);
  registerShortcut(_zoomYLocalMaximum);
  connect(_zoomYLocalMaximum, SIGNAL(triggered()), this, SLOT(zoomYLocalMaximum()));

  _zoomYMaximum = new QAction(tr("Y-Zoom Maximum"), this);
  _zoomYMaximum->setShortcut(Qt::SHIFT+Qt::Key_M);
  registerShortcut(_zoomYMaximum);
  connect(_zoomYMaximum, SIGNAL(triggered()), this, SLOT(zoomYMaximum()));

  _zoomYUp= new QAction(tr("Y-Zoom Up"), this);
  _zoomYUp->setShortcut(Qt::Key_Up);
  registerShortcut(_zoomYUp);
  connect(_zoomYUp, SIGNAL(triggered()), this, SLOT(zoomYUp()));

  _zoomYDown= new QAction(tr("Y-Zoom Down"), this);
  _zoomYDown->setShortcut(Qt::Key_Down);
  registerShortcut(_zoomYDown);
  connect(_zoomYDown, SIGNAL(triggered()), this, SLOT(zoomYDown()));

  _zoomYOut = new QAction(tr("Y-Zoom Out"), this);
  _zoomYOut->setShortcut(Qt::SHIFT+Qt::Key_Up);
  registerShortcut(_zoomYOut);
  connect(_zoomYOut, SIGNAL(triggered()), this, SLOT(zoomYOut()));

  _zoomYIn = new QAction(tr("Y-Zoom In"), this);
  _zoomYIn->setShortcut(Qt::SHIFT+Qt::Key_Down);
  registerShortcut(_zoomYIn);
  connect(_zoomYIn, SIGNAL(triggered()), this, SLOT(zoomYIn()));

  _zoomNormalizeYtoX = new QAction(tr("Normalize Y Axis to X Axis"), this);
  _zoomNormalizeYtoX->setShortcut(Qt::SHIFT+Qt::Key_N);
  registerShortcut(_zoomNormalizeYtoX);
  connect(_zoomNormalizeYtoX, SIGNAL(triggered()), this, SLOT(zoomNormalizeYtoX()));

  _zoomLogY = new QAction(tr("Log Y Axis"), this);
  _zoomLogY->setShortcut(Qt::Key_L);
  _zoomLogY->setCheckable(true);
  registerShortcut(_zoomLogY);
  connect(_zoomLogY, SIGNAL(triggered()), this, SLOT(zoomLogY()));
}


void PlotRenderItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
  QMenu menu;

  addTitle(&menu);

  QAction *editAction = menu.addAction(tr("Edit"));
  connect(editAction, SIGNAL(triggered()), this, SLOT(edit()));

  QMenu zoom;
  zoom.setTitle(tr("Zoom"));

  zoom.addAction(_zoomMaximum);
  zoom.addAction(_zoomMaxSpikeInsensitive);
  zoom.addAction(_zoomYMeanCentered);

  zoom.addSeparator();

  zoom.addAction(_zoomXMaximum);
  zoom.addAction(_zoomXRight);
  zoom.addAction(_zoomXLeft);
  zoom.addAction(_zoomXOut);
  zoom.addAction(_zoomXIn);
  zoom.addAction(_zoomNormalizeXtoY);
  zoom.addAction(_zoomLogX);

  zoom.addSeparator();

  zoom.addAction(_zoomYLocalMaximum);
  zoom.addAction(_zoomYMaximum);
  zoom.addAction(_zoomYUp);
  zoom.addAction(_zoomYDown);
  zoom.addAction(_zoomYOut);
  zoom.addAction(_zoomYIn);
  zoom.addAction(_zoomNormalizeYtoX);
  zoom.addAction(_zoomLogY);

  menu.addMenu(&zoom);

  menu.exec(event->screenPos());
}


void PlotRenderItem::keyPressEvent(QKeyEvent *event) {
  const Qt::KeyboardModifiers modifiers = QApplication::keyboardModifiers();
  if (modifiers & Qt::ShiftModifier) {
    setCursor(Qt::SizeVerCursor);
  _selectionRect.setFrom(QPointF(rect().left(), _lastPos.y()));
  _selectionRect.setTo(QPointF(rect().right(), _lastPos.y()));
  } else if (modifiers & Qt::ControlModifier) {
    setCursor(Qt::SizeHorCursor);
  _selectionRect.setFrom(QPointF(_lastPos.x(), rect().top()));
  _selectionRect.setTo(QPointF(_lastPos.x(), rect().bottom()));
  }
  ViewItem::keyPressEvent(event);

  updateSelectionRect();
}


void PlotRenderItem::keyReleaseEvent(QKeyEvent *event) {
  const Qt::KeyboardModifiers modifiers = QApplication::keyboardModifiers();
  if (modifiers & Qt::ShiftModifier) {
   setCursor(Qt::SizeVerCursor);
  } else if (modifiers & Qt::ControlModifier) {
    setCursor(Qt::SizeHorCursor);
  } else {
    resetSelectionRect();
  }
  ViewItem::keyReleaseEvent(event);
}


void PlotRenderItem::resetSelectionRect() {
  if (_selectionRect.isValid()) {
    _selectionRect.reset();
    updateCursor(_lastPos);
    update();
  }
}


void PlotRenderItem::updateSelectionRect() {
  if (_selectionRect.isValid()) {
    update(); //FIXME should optimize instead of redrawing entire curve?
  }
}


void PlotRenderItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
  if (parentView()->viewMode() != View::Data) {
    event->ignore();
    return;
  }

  const QPointF p = event->pos();
  const Qt::KeyboardModifiers modifiers = QApplication::keyboardModifiers();
  if (modifiers & Qt::ShiftModifier) {
    _selectionRect.setTo(QPointF(rect().right(), p.y()));
  } else if (modifiers & Qt::ControlModifier) {
    _selectionRect.setTo(QPointF(p.x(), rect().bottom()));
  } else {
    _selectionRect.setTo(p);
  }

  updateSelectionRect();
}


void PlotRenderItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
  if (parentView()->viewMode() != View::Data) {
    event->ignore();
    return;
  }

  if (checkBox().contains(event->pos())) {
    setTiedZoom(!isTiedZoom());
    update(); //FIXME should optimize instead of redrawing entire curve!
  }

  const QPointF p = event->pos();
  const Qt::KeyboardModifiers modifiers = QApplication::keyboardModifiers();
  if (modifiers & Qt::ShiftModifier) {
    setCursor(Qt::SizeVerCursor);
    _selectionRect.setFrom(QPointF(rect().left(), p.y()));
    _selectionRect.setTo(QPointF(rect().right(), p.y()));
  } else if (modifiers & Qt::ControlModifier) {
    setCursor(Qt::SizeHorCursor);
    _selectionRect.setFrom(QPointF(p.x(), rect().top()));
    _selectionRect.setTo(QPointF(p.x(), rect().bottom()));
  } else {
    _selectionRect.setFrom(p);
  }
}


void PlotRenderItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
  if (parentView()->viewMode() != View::Data) {
    event->ignore();
    return;
  }

  updateCursor(event->pos());
  const QRectF projection = plotItem()->mapToProjection(_selectionRect.rect());
  _selectionRect.reset();

  zoomFixedExpression(projection);
}


void PlotRenderItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
  ViewItem::hoverMoveEvent(event);

  const QPointF p = event->pos();
  const Qt::KeyboardModifiers modifiers = QApplication::keyboardModifiers();
  if (modifiers & Qt::ShiftModifier) {
    _lastPos = p;
    setCursor(Qt::SizeVerCursor);
    _selectionRect.setFrom(QPointF(rect().left(), p.y()));
    _selectionRect.setTo(QPointF(rect().right(), p.y()));
    update(); //FIXME should optimize instead of redrawing entire curve!
  } else if (modifiers & Qt::ControlModifier) {
    _lastPos = p;
    setCursor(Qt::SizeHorCursor);
    _selectionRect.setFrom(QPointF(p.x(), rect().top()));
    _selectionRect.setTo(QPointF(p.x(), rect().bottom()));
    update(); //FIXME should optimize instead of redrawing entire curve!
  } else {
    resetSelectionRect();
  }
  const QPointF point = plotItem()->mapToProjection(event->pos());
  QString message = QString("(%1, %2)").arg(QString::number(point.x(), 'G')).arg(QString::number(point.y()));
  kstApp->mainWindow()->statusBar()->showMessage(message);
}


void PlotRenderItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
  ViewItem::hoverEnterEvent(event);

  updateCursor(event->pos());

  const QPointF p = plotItem()->mapToProjection(event->pos());
  QString message = QString("(%1, %2)").arg(QString::number(p.x())).arg(QString::number(p.y()));
  kstApp->mainWindow()->statusBar()->showMessage(message);
}


void PlotRenderItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
  ViewItem::hoverLeaveEvent(event);

  updateCursor(event->pos());

  kstApp->mainWindow()->statusBar()->showMessage(QString());
}


void PlotRenderItem::zoomFixedExpression(const QRectF &projection) {
  qDebug() << "zoomFixedExpression" << endl;
  ZoomCommand *cmd = new ZoomFixedExpressionCommand(this, projection);
  cmd->redo();
}


void PlotRenderItem::zoomMaximum() {
  qDebug() << "zoomMaximum" << endl;
  ZoomCommand *cmd = new ZoomMaximumCommand(this);
  cmd->redo();
}


void PlotRenderItem::zoomMaxSpikeInsensitive() {
  qDebug() << "zoomMaxSpikeInsensitive" << endl;
  ZoomCommand *cmd = new ZoomMaxSpikeInsensitiveCommand(this);
  cmd->redo();
}


void PlotRenderItem::zoomYMeanCentered() {
  qDebug() << "zoomYMeanCentered" << endl;
  ZoomCommand *cmd = new ZoomYMeanCenteredCommand(this);
  cmd->redo();
}


void PlotRenderItem::zoomXMaximum() {
  qDebug() << "zoomXMaximum" << endl;
  ZoomCommand *cmd = new ZoomXMaximumCommand(this);
  cmd->redo();
}


void PlotRenderItem::zoomXRight() {
  qDebug() << "zoomXRight" << endl;
  ZoomCommand *cmd = new ZoomXRightCommand(this);
  cmd->redo();
}


void PlotRenderItem::zoomXLeft() {
  qDebug() << "zoomXLeft" << endl;
  ZoomCommand *cmd = new ZoomXLeftCommand(this);
  cmd->redo();
}


void PlotRenderItem::zoomXOut() {
  qDebug() << "zoomXOut" << endl;
  resetSelectionRect();
  ZoomCommand *cmd = new ZoomXOutCommand(this);
  cmd->redo();
}


void PlotRenderItem::zoomXIn() {
  qDebug() << "zoomXIn" << endl;
  resetSelectionRect();
  ZoomCommand *cmd = new ZoomXInCommand(this);
  cmd->redo();
}


void PlotRenderItem::zoomNormalizeXtoY() {
  qDebug() << "zoomNormalizeXtoY" << endl;

  if (plotItem()->xAxisLog() || plotItem()->yAxisLog())
    return; //apparently we don't want to do anything here according to kst2dplot...

  ZoomCommand *cmd = new ZoomNormalizeXToYCommand(this);
  cmd->redo();
}


void PlotRenderItem::zoomLogX() {
  qDebug() << "zoomLogX" << endl;
  plotItem()->setXAxisLog(_zoomLogX->isChecked());
  setProjectionRect(computedProjectionRect()); //need to recompute
  plotItem()->update();
}


void PlotRenderItem::zoomYLocalMaximum() {
  qDebug() << "zoomYLocalMaximum" << endl;
  ZoomCommand *cmd = new ZoomYLocalMaximumCommand(this);
  cmd->redo();
}


void PlotRenderItem::zoomYMaximum() {
  qDebug() << "zoomYMaximum" << endl;
  ZoomCommand *cmd = new ZoomYMaximumCommand(this);
  cmd->redo();
}


void PlotRenderItem::zoomYUp() {
  qDebug() << "zoomYUp" << endl;
  ZoomCommand *cmd = new ZoomYUpCommand(this);
  cmd->redo();
}


void PlotRenderItem::zoomYDown() {
  qDebug() << "zoomYDown" << endl;
  ZoomCommand *cmd = new ZoomYDownCommand(this);
  cmd->redo();
}


void PlotRenderItem::zoomYOut() {
  qDebug() << "zoomYOut" << endl;
  resetSelectionRect();
  ZoomCommand *cmd = new ZoomYOutCommand(this);
  cmd->redo();
}


void PlotRenderItem::zoomYIn() {
  qDebug() << "zoomYIn" << endl;
  resetSelectionRect();
  ZoomCommand *cmd = new ZoomYInCommand(this);
  cmd->redo();
}


void PlotRenderItem::zoomNormalizeYtoX() {
  qDebug() << "zoomNormalizeYtoX" << endl;

  if (plotItem()->xAxisLog() || plotItem()->yAxisLog())
    return; //apparently we don't want to do anything here according to kst2dplot...

  ZoomCommand *cmd = new ZoomNormalizeYToXCommand(this);
  cmd->redo();
}


void PlotRenderItem::zoomLogY() {
  qDebug() << "zoomLogY" << endl;
  plotItem()->setYAxisLog(_zoomLogY->isChecked());
  setProjectionRect(computedProjectionRect()); //need to recompute
  plotItem()->update();
}


QPainterPath PlotRenderItem::shape() const {
  QPainterPath selectPath;
  selectPath.setFillRule(Qt::WindingFill);
  selectPath.addPolygon(rect());
  selectPath.addPath(checkBox());
  return selectPath;
}


QRectF PlotRenderItem::boundingRect() const {
  QPolygonF checkBound = checkBoxBoundingRect();
  return QRectF(checkBound[0], checkBound[2]);
}


QSizeF PlotRenderItem::sizeOfGrip() const {
  return ViewItem::sizeOfGrip() / 1.2;
}


bool PlotRenderItem::maybeReparent() {
  return false; //never reparent a plot renderer
}


QRectF PlotRenderItem::checkBoxBoundingRect() const {
  QRectF bound = selectBoundingRect();
  bound.setTopLeft(bound.topLeft() - QPointF(sizeOfGrip().width(), sizeOfGrip().height()));
  bound.setWidth(bound.width() + sizeOfGrip().width());
  bound.setHeight(bound.height() + sizeOfGrip().height());
  return bound;
}


QPainterPath PlotRenderItem::checkBox() const {
  QRectF bound = checkBoxBoundingRect();
  QRectF grip = QRectF(bound.topRight() - QPointF(sizeOfGrip().width(), 0), sizeOfGrip());
  QPainterPath path;
  path.addEllipse(grip);
  return path;
}


void PlotRenderItem::updateGeometry() {
  setViewRect(plotItem()->plotRect());
}


void PlotRenderItem::updateViewMode() {
  switch (parentView()->viewMode()) {
  case View::Data:
    setCursor(Qt::CrossCursor);
    break;
  case View::Layout:
    setCursor(Qt::ArrowCursor);
    break;
  default:
    break;
  }
}


void PlotRenderItem::edit() {
  plotItem()->edit();
}


void PlotRenderItem::updateCursor(const QPointF &pos) {
  _lastPos = pos;
  if (checkBox().contains(pos)) {
    setCursor(Qt::ArrowCursor);
  } else {
    updateViewMode();
  }
}


ZoomState PlotRenderItem::currentZoomState() {
  ZoomState zoomState;
  zoomState.item = this; //the origin of this ZoomState
  zoomState.projectionRect = projectionRect();
  zoomState.xAxisZoomMode = xAxisZoomMode();
  zoomState.yAxisZoomMode = yAxisZoomMode();
  zoomState.isXAxisLog = plotItem()->xAxisLog();
  zoomState.isYAxisLog = plotItem()->yAxisLog();
  zoomState.xLogBase = 10.0;
  zoomState.yLogBase = 10.0;
  return zoomState;
}


void PlotRenderItem::setCurrentZoomState(ZoomState zoomState) {
  setXAxisZoomMode(ZoomMode(zoomState.xAxisZoomMode));
  setYAxisZoomMode(ZoomMode(zoomState.yAxisZoomMode));
  plotItem()->setXAxisLog(zoomState.isXAxisLog);
  plotItem()->setYAxisLog(zoomState.isYAxisLog);
  setProjectionRect(zoomState.projectionRect);
}


QRectF PlotRenderItem::computedProjectionRect() const {
  qreal minX, minY, maxX, maxY;

  //initialize to current projection rect...
  projectionRect().getCoords(&minX, &minY, &maxX, &maxY);

  computeXAxisRange(&minX, &maxX);
  computeYAxisRange(&minY, &maxY);

  return QRectF(QPointF(minX, minY),
                QPointF(maxX, maxY));
}


void PlotRenderItem::computeXAxisRange(qreal *min, qreal *max) const {
  qreal minimum = *min;
  qreal maximum = *max;

  switch (_xAxisZoomMode) {
  case Auto:
    computeAuto(Qt::Horizontal, &minimum, &maximum);
    break;
  case AutoBorder: //auto mode, plus a 2.5% border on top and bottom.
    computeAuto(Qt::Horizontal, &minimum, &maximum);
    computeBorder(Qt::Horizontal, &minimum, &maximum);
    break;
  case FixedExpression: //limits are given by scalar equations, or mouse
    qDebug() << "FIXME! Need a GUI for entering scalar equations..." << endl;
    break;
  case SpikeInsensitive: //auto with algorithm to detect spikes TBD
    qDebug() << "FIXME! Need a spike insensitive algorithm..." << endl;
    break;
  case MeanCentered: //the mean of all active curves
    computeMeanCentered(Qt::Horizontal, &minimum, &maximum);
    break;
  default:
    break;
  }

  *min = minimum;
  *max = maximum;
}


void PlotRenderItem::computeYAxisRange(qreal *min, qreal *max) const {
  qreal minimum = *min;
  qreal maximum = *max;

  switch (_yAxisZoomMode) {
  case Auto:
    computeAuto(Qt::Vertical, &minimum, &maximum);
    break;
  case AutoBorder: //auto mode, plus a 2.5% border on top and bottom.
    computeAuto(Qt::Vertical, &minimum, &maximum);
    computeBorder(Qt::Vertical, &minimum, &maximum);
    break;
  case FixedExpression: //limits are given by scalar equations, or mouse
    qDebug() << "FIXME! Need a GUI for entering scalar equations..." << endl;
    break;
  case SpikeInsensitive: //auto with algorithm to detect spikes TBD
    qDebug() << "FIXME! Need a spike insensitive algorithm..." << endl;
    break;
  case MeanCentered: //the mean of all active curves
    computeMeanCentered(Qt::Vertical, &minimum, &maximum);
    break;
  default:
    break;
  }

  *min = minimum;
  *max = maximum;
}


void PlotRenderItem::computeAuto(Qt::Orientation orientation, qreal *min, qreal *max) const {
  //The previous values are of no consequence as this algorithm does not depend
  //on the previous values.  So start over so that first active relation initializes.
  qreal minimum;
  qreal maximum;
  bool unInitialized = true;

  bool axisLog = orientation == Qt::Horizontal ? plotItem()->xAxisLog() : plotItem()->yAxisLog();

  foreach (RelationPtr relation, relationList()) {
      if (relation->ignoreAutoScale())
        continue;

      qreal minPos_ = orientation == Qt::Horizontal ? relation->minPosX() : relation->minPosY();
      qreal min_ = orientation == Qt::Horizontal ? relation->minX() : relation->minY();
      qreal max_ = orientation == Qt::Horizontal ? relation->maxX() : relation->maxY();

      //If the axis is in log mode, the lower extent will be the
      //minimum value larger than zero.
      if (axisLog)
        minimum = unInitialized ? minPos_ : qMin(minPos_, minimum);
      else
        minimum = unInitialized ? min_ : qMin(min_, minimum);

      maximum = unInitialized ? max_ : qMax(max_, maximum);

      unInitialized = false;
  }

  if (unInitialized || maximum <= minimum) {
    minimum = axisLog ? 0.0 : -0.1;
    minimum = 0.1;
  }

  *min = minimum;
  *max = maximum;
}


void PlotRenderItem::computeBorder(Qt::Orientation orientation, qreal *min, qreal *max) const {
  qreal minimum = *min;
  qreal maximum = *max;

  bool axisLog = orientation == Qt::Horizontal ? plotItem()->xAxisLog() : plotItem()->yAxisLog();
  qreal logBase = 10.0/*orientation == Qt::Horizontal ? xLogBase() : yLogBase()*/;

  if (axisLog) {
    minimum = log10(minimum)/log10(logBase);
    maximum = maximum > 0.0 ? log10(maximum) : 0.0;
    qreal d = qAbs(maximum - minimum) * 0.025;
    maximum = pow(logBase, maximum + d);
    minimum = pow(logBase, minimum - d);
  } else {
    qreal d = qAbs(maximum - minimum) * 0.025;
    maximum += d;
    minimum -= d;
  }

  *min = minimum;
  *max = maximum;
}


void PlotRenderItem::computeMeanCentered(Qt::Orientation orientation, qreal *min, qreal *max) const {
  qreal minimum = *min;
  qreal maximum = *max;

  int count = 0;
  qreal mid = 0.0;

  foreach (RelationPtr relation, relationList()) {
      if (relation->ignoreAutoScale())
        continue;

      mid += orientation == Qt::Horizontal ? relation->midX() : relation->midY();
      ++count;
  }

  if (count) {
    mid /= qreal(count);
    qreal delta = maximum - minimum;
    minimum = mid - delta / 2.0;
    maximum = mid + delta / 2.0;
  }

  *min = minimum;
  *max = maximum;
}


ZoomCommand::ZoomCommand(PlotRenderItem *item, const QString &text)
    : ViewItemCommand(item, text) {

  if (!item->isTiedZoom()) {
    _originalStates << item->currentZoomState();
  } else {
    QList<PlotItem*> plots = PlotItemManager::tiedZoomPlotsForView(item->parentView());
    foreach (PlotItem *plotItem, plots) {
      QList<PlotRenderItem *> renderers = plotItem->renderItems();
      foreach (PlotRenderItem *renderItem, renderers) {
        _originalStates << renderItem->currentZoomState();
      }
    }
  }
}


ZoomCommand::~ZoomCommand() {
}


void ZoomCommand::undo() {
  foreach (ZoomState state, _originalStates) {
    state.item->setCurrentZoomState(state);
  }
}


void ZoomCommand::redo() {
  foreach (ZoomState state, _originalStates) {
    applyZoomTo(state.item);
  }
}


/*
 * X axis zoom to FixedExpression, Y axis zoom to FixedExpression.
 */
void ZoomFixedExpressionCommand::applyZoomTo(PlotRenderItem *item) {
  item->setXAxisZoomMode(PlotRenderItem::FixedExpression);
  item->setYAxisZoomMode(PlotRenderItem::FixedExpression);
  item->setProjectionRect(_fixed);
}


/*
 * X axis zoom to Auto, Y axis zoom to AutoBorder.
 */
void ZoomMaximumCommand::applyZoomTo(PlotRenderItem *item) {
  item->setXAxisZoomMode(PlotRenderItem::Auto);
  item->setYAxisZoomMode(PlotRenderItem::AutoBorder);
  item->setProjectionRect(item->computedProjectionRect());
}


/*
 * X axis zoom to Auto, Y axis zoom to SpikeInsensitive.
 */
void ZoomMaxSpikeInsensitiveCommand::applyZoomTo(PlotRenderItem *item) {
  item->setXAxisZoomMode(PlotRenderItem::Auto);
  item->setYAxisZoomMode(PlotRenderItem::SpikeInsensitive);
  item->setProjectionRect(item->computedProjectionRect());
}


/*
 * X axis zoom to Auto, Y axis zoom to Mean Centered.
 */
void ZoomYMeanCenteredCommand::applyZoomTo(PlotRenderItem *item) {
  item->setXAxisZoomMode(PlotRenderItem::Auto);
  item->setYAxisZoomMode(PlotRenderItem::MeanCentered);
  item->setProjectionRect(item->computedProjectionRect());
}


/*
 * X axis zoom to auto, Y zoom not changed.
 */
void ZoomXMaximumCommand::applyZoomTo(PlotRenderItem *item) {
  item->setXAxisZoomMode(PlotRenderItem::Auto);
  QRectF compute = item->computedProjectionRect();
  item->setProjectionRect(QRectF(compute.x(),
                           item->projectionRect().y(),
                           compute.width(),
                           item->projectionRect().height()));
}

/*
 * X axis zoom changed to fixed and shifted to right:
 *       new_xmin = xmin + (xmax - xmin)*0.10;
 *       new_xmax = xmax + (xmax – xmin)*0.10;
 */
void ZoomXRightCommand::applyZoomTo(PlotRenderItem *item) {
  item->setXAxisZoomMode(PlotRenderItem::FixedExpression);

  QRectF compute = item->projectionRect();

  qreal dx = (item->plotItem()->xMax() - item->plotItem()->xMin())*0.10;
  if (item->plotItem()->xAxisLog()) { 
    compute.setLeft(pow(10, item->plotItem()->xMin() + dx));
    compute.setRight(pow(10, item->plotItem()->xMax() + dx));
  } else {
    compute.setLeft(compute.left() + dx);
    compute.setRight(compute.right() + dx);
  }

  item->setProjectionRect(compute);
  item->update();
}

/*
 * X axis zoom changed to fixed and shifted to :
 *       new_xmin = xmin - (xmax - xmin)*0.10;
 *       new_xmax = xmax - (xmax – xmin)*0.10;
 */
void ZoomXLeftCommand::applyZoomTo(PlotRenderItem *item) {
  item->setXAxisZoomMode(PlotRenderItem::FixedExpression);

  QRectF compute = item->projectionRect();

  qreal dx = (item->plotItem()->xMax() - item->plotItem()->xMin())*0.10;
  if (item->plotItem()->xAxisLog()) { 
    compute.setLeft(pow(10, item->plotItem()->xMin() - dx));
    compute.setRight(pow(10, item->plotItem()->xMax() - dx));
  } else {
    compute.setLeft(compute.left() - dx);
    compute.setRight(compute.right() - dx);
  }

  item->setProjectionRect(compute);
  item->update();
}

/*
 * X axis zoom changed to fixed and increased:
 *       new_xmin = xmin - (xmax - xmin)*0.25;
 *       new_xmax = xmax + (xmax – xmin)*0.25;
 */
void ZoomXOutCommand::applyZoomTo(PlotRenderItem *item) {
  item->setXAxisZoomMode(PlotRenderItem::FixedExpression);

  QRectF compute = item->projectionRect();

  qreal dx = (item->plotItem()->xMax() - item->plotItem()->xMin())*0.25;
  if (item->plotItem()->xAxisLog()) { 
    compute.setLeft(pow(10, item->plotItem()->xMin() - dx));
    compute.setRight(pow(10, item->plotItem()->xMax() + dx));
  } else {
    compute.setLeft(compute.left() - dx);
    compute.setRight(compute.right() + dx);
  }

  item->setProjectionRect(compute);
  item->update();
}


/*
 * X axis zoom changed to fixed and decreased:
 *       new_xmin = xmin + (xmax - xmin)*0.1666666;
 *       new_xmax = xmax - (xmax – xmin)*0.1666666;
 */
void ZoomXInCommand::applyZoomTo(PlotRenderItem *item) {
  item->setXAxisZoomMode(PlotRenderItem::FixedExpression);

  QRectF compute = item->projectionRect();

  qreal dx = (item->plotItem()->xMax() - item->plotItem()->xMin())*0.1666666;
  if (item->plotItem()->xAxisLog()) { 
    compute.setLeft(pow(10, item->plotItem()->xMin() + dx));
    compute.setRight(pow(10, item->plotItem()->xMax() - dx));
  } else {
    compute.setLeft(compute.left() + dx);
    compute.setRight(compute.right() - dx);
  }

  item->setProjectionRect(compute);
}


/*
 * Normalize X axis to Y axis: Given the current plot aspect ratio, change
 * the X axis range to have the same units per mm as the Y axis range. Particularly
 * useful for images.
 */
void ZoomNormalizeXToYCommand::applyZoomTo(PlotRenderItem *item) {
  QRectF compute = item->projectionRect();
  qreal mean = compute.center().x();
  qreal range = item->plotRect().width() * compute.height() / item->plotRect().height();

  compute.setLeft(mean - (range / 2.0));
  compute.setRight(mean + (range / 2.0));

  item->setXAxisZoomMode(PlotRenderItem::FixedExpression);
  item->setProjectionRect(compute);
}


/*
 * When zoomed in in X, auto zoom Y, only
 * counting points within the current X range. (eg, curve goes from x=0 to 100, but
 * we are zoomed in to x = 30 to 40. Adjust Y zoom to include all points with x
 * values between 30 and 40.
 */
void ZoomYLocalMaximumCommand::applyZoomTo(PlotRenderItem *item) {
  qreal minimum = item->plotItem()->yAxisLog() ? 0.0 : -0.1;
  qreal maximum = 0.1;
  foreach (RelationPtr relation, item->relationList()) {
      if (relation->ignoreAutoScale())
        continue;

      qreal min, max;
      relation->yRange(item->projectionRect().left(),
                       item->projectionRect().right(),
                       &min, &max);

      //If the axis is in log mode, the lower extent will be the
      //minimum value larger than zero.
      if (item->plotItem()->yAxisLog())
        minimum = minimum <= 0.0 ? min : qMin(min, minimum);
      else
        minimum = qMin(min, minimum);

      maximum = qMax(max, maximum);
  }

  item->computeBorder(Qt::Vertical, &minimum, &maximum);

  item->setYAxisZoomMode(PlotRenderItem::FixedExpression);

  QRectF compute = item->projectionRect();
  compute.setTop(minimum);
  compute.setBottom(maximum);

  item->setProjectionRect(compute);
}


/*
 * Y axis zoom to auto, X zoom not changed.
 */
void ZoomYMaximumCommand::applyZoomTo(PlotRenderItem *item) {
  item->setYAxisZoomMode(PlotRenderItem::Auto);
  QRectF compute = item->computedProjectionRect();
  item->setProjectionRect(QRectF(item->projectionRect().x(),
                           compute.y(),
                           item->projectionRect().width(),
                           compute.height()));
}


/*
 * Y axis zoom up. If the Y zoom mode is not
 * Mean Centered, change to Fixed (expression).
 *             new_ymin = ymin + (ymax - ymin)*0.1;
 *             new_ymax = ymax + (ymax - ymin)*0.1;
 */
void ZoomYUpCommand::applyZoomTo(PlotRenderItem *item) {
  if (item->yAxisZoomMode() != PlotRenderItem::MeanCentered)
    item->setYAxisZoomMode(PlotRenderItem::FixedExpression);

  QRectF compute = item->projectionRect();

  qreal dy = (item->plotItem()->yMax() - item->plotItem()->yMin())*0.1;
  if (item->plotItem()->yAxisLog()) { 
    compute.setTop(pow(10, item->plotItem()->yMin() + dy));
    compute.setBottom(pow(10, item->plotItem()->yMax() + dy));
  } else {
    compute.setTop(compute.top() + dy);
    compute.setBottom(compute.bottom() + dy);
  }

  item->setProjectionRect(compute);
}


/*
 * Y axis zoom down. If the Y zoom mode is not
 * Mean Centered, change to Fixed (expression).
 *             new_ymin = ymin - (ymax - ymin)*0.10;
 *             new_ymax = ymax - (ymax - ymin)*0.10;
 */
void ZoomYDownCommand::applyZoomTo(PlotRenderItem *item) {
  if (item->yAxisZoomMode() != PlotRenderItem::MeanCentered)
    item->setYAxisZoomMode(PlotRenderItem::FixedExpression);

  QRectF compute = item->projectionRect();

  qreal dy = (item->plotItem()->yMax() - item->plotItem()->yMin())*0.1;
  if (item->plotItem()->yAxisLog()) { 
    compute.setTop(pow(10, item->plotItem()->yMin() - dy));
    compute.setBottom(pow(10, item->plotItem()->yMax() - dy));
  } else {
    compute.setTop(compute.top() - dy);
    compute.setBottom(compute.bottom() - dy);
  }

  item->setProjectionRect(compute);
}


/*
 * Y axis zoom increased. If the Y zoom mode is not
 * Mean Centered, change to Fixed (expression).
 *             new_ymin = ymin - (ymax - ymin)*0.25;
 *             new_ymax = ymax + (ymax - ymin)*0.25;
 */
void ZoomYOutCommand::applyZoomTo(PlotRenderItem *item) {
  if (item->yAxisZoomMode() != PlotRenderItem::MeanCentered)
    item->setYAxisZoomMode(PlotRenderItem::FixedExpression);

  QRectF compute = item->projectionRect();

  qreal dy = (item->plotItem()->yMax() - item->plotItem()->yMin())*0.25;
  if (item->plotItem()->yAxisLog()) { 
    compute.setTop(pow(10, item->plotItem()->yMin() - dy));
    compute.setBottom(pow(10, item->plotItem()->yMax() + dy));
  } else {
    compute.setTop(compute.top() - dy);
    compute.setBottom(compute.bottom() + dy);
  }

  item->setProjectionRect(compute);
  item->update();
}


/*
 * Y axis zoom decreased. If the Y zoom mode is not
 * Mean Centered, change to Fixed (expression).
 *             new_ymin = ymin + (ymax - ymin)*0.1666666;
 *             new_ymax = ymax - (ymax – ymin)*0.1666666;
 */
void ZoomYInCommand::applyZoomTo(PlotRenderItem *item) {
  if (item->yAxisZoomMode() != PlotRenderItem::MeanCentered)
    item->setYAxisZoomMode(PlotRenderItem::FixedExpression);

  QRectF compute = item->projectionRect();

  qreal dy = (item->plotItem()->yMax() - item->plotItem()->yMin())*0.1666666;
  if (item->plotItem()->yAxisLog()) { 
    compute.setTop(pow(10, item->plotItem()->yMin() + dy));
    compute.setBottom(pow(10, item->plotItem()->yMax() - dy));
  } else {
    compute.setTop(compute.top() + dy);
    compute.setBottom(compute.bottom() - dy);
  }

  item->setProjectionRect(compute);
  item->update();
}


/*
 * Normalize Y axis to X axis: Given the current plot aspect ratio,
 * change the Y axis range to have the same units per mm as the X axis range.
 * Particularly useful for images.
 */
void ZoomNormalizeYToXCommand::applyZoomTo(PlotRenderItem *item) {
  QRectF compute = item->projectionRect();
  qreal mean = compute.center().y();
  qreal range = item->plotRect().height() * compute.width() / item->plotRect().width();

  compute.setTop(mean - (range / 2.0));
  compute.setBottom(mean + (range / 2.0));

  item->setYAxisZoomMode(PlotRenderItem::FixedExpression);
  item->setProjectionRect(compute);
}

}

// vim: ts=2 sw=2 et
