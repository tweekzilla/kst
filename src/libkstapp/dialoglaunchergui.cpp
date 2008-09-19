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

#include "dialoglaunchergui.h"

#include "application.h"
#include "curvedialog.h"
#include "equationdialog.h"
#include "histogramdialog.h"
#include "vectordialog.h"
#include "scalardialog.h"
#include "matrixdialog.h"
#include "powerspectrumdialog.h"
#include "csddialog.h"
#include "imagedialog.h"
#include "eventmonitordialog.h"
#include "basicplugindialog.h"
#include "filterfitdialog.h"

namespace Kst {

DialogLauncherGui::DialogLauncherGui() {
}


DialogLauncherGui::~DialogLauncherGui() {
}


void DialogLauncherGui::showVectorDialog(QString &vectorname, ObjectPtr objectPtr) {
  VectorDialog dialog(objectPtr, kstApp->mainWindow());
  dialog.exec();
  vectorname = dialog.dataObjectName();
}


void DialogLauncherGui::showMatrixDialog(QString &matrixName, ObjectPtr objectPtr) {
  MatrixDialog dialog(objectPtr, kstApp->mainWindow());
  dialog.exec();
  matrixName = dialog.dataObjectName();
}


void DialogLauncherGui::showScalarDialog(QString &scalarname, ObjectPtr objectPtr) {
  ScalarDialog dialog(objectPtr, kstApp->mainWindow());
  dialog.exec();
  scalarname = dialog.dataObjectName();
}


void DialogLauncherGui::showStringDialog(ObjectPtr objectPtr) {
  Q_UNUSED(objectPtr);
}


void DialogLauncherGui::showCurveDialog(ObjectPtr objectPtr, VectorPtr vector) {
  CurveDialog dialog(objectPtr, kstApp->mainWindow());
  if (vector) {
    dialog.setVector(vector);
  }
  dialog.exec();
}


void DialogLauncherGui::showImageDialog(ObjectPtr objectPtr, MatrixPtr matrix) {
  ImageDialog dialog(objectPtr, kstApp->mainWindow());
  if (matrix) {
    dialog.setMatrix(matrix);
  }
  dialog.exec();
}


void DialogLauncherGui::showEquationDialog(ObjectPtr objectPtr) {
  EquationDialog(objectPtr, kstApp->mainWindow()).exec();
}


void DialogLauncherGui::showHistogramDialog(ObjectPtr objectPtr, VectorPtr vector) {
  HistogramDialog dialog(objectPtr, kstApp->mainWindow());
  if (vector) {
    dialog.setVector(vector);
  }
  dialog.exec();
}


void DialogLauncherGui::showPowerSpectrumDialog(ObjectPtr objectPtr, VectorPtr vector) {
  PowerSpectrumDialog dialog(objectPtr, kstApp->mainWindow());
  if (vector) {
    dialog.setVector(vector);
  }
  dialog.exec();
}


void DialogLauncherGui::showCSDDialog(ObjectPtr objectPtr, VectorPtr vector) {
  CSDDialog dialog(objectPtr, kstApp->mainWindow());
  if (vector) {
    dialog.setVector(vector);
  }
  dialog.exec();
}


void DialogLauncherGui::showEventMonitorDialog(ObjectPtr objectPtr) {
  EventMonitorDialog dialog(objectPtr, kstApp->mainWindow());
  dialog.exec();
}


void DialogLauncherGui::showBasicPluginDialog(QString pluginName, ObjectPtr objectPtr, VectorPtr vectorX, VectorPtr vectorY, PlotItemInterface *plotItem) {
  if (DataObject::pluginType(pluginName) == DataObjectPluginInterface::Generic) {
    BasicPluginDialog dialog(pluginName, objectPtr, kstApp->mainWindow());
    dialog.exec();
  } else {
    FilterFitDialog dialog(pluginName, objectPtr, kstApp->mainWindow());
    if (!objectPtr) {
      if (vectorX) {
        dialog.setVectorX(vectorX);
      }
      if (vectorY) {
        dialog.setVectorY(vectorY);
      }
      if (plotItem) {
        dialog.setPlotMode((PlotItem*)plotItem);
      }
    }
    dialog.exec();
  }
}
}

// vim: ts=2 sw=2 et
