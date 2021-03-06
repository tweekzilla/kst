/***************************************************************************
 *                                                                         *
 *   copyright : (C) 2007 The University of Toronto                        *
 *                   netterfield@astro.utoronto.ca                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef DATAWIZARD_H
#define DATAWIZARD_H

#include <QWizard>
#include <QThread>

#include "kst_export.h"

#include "ui_datawizardpagedatasource.h"
#include "ui_datawizardpagevectors.h"
#include "ui_datawizardpagefilters.h"
#include "ui_datawizardpageplot.h"
#include "ui_datawizardpagedatapresentation.h"

#include "datasource.h"
#include "curveplacement.h"

namespace Kst {

class Document;
class ObjectStore;
class PlotItemInterface;
class ValidateDataSourceThread;

class DataWizardPageDataSource : public QWizardPage, Ui::DataWizardPageDataSource
{
  Q_OBJECT
  public:
    DataWizardPageDataSource(ObjectStore *store, QWidget *parent, const QString& filename);
    virtual ~DataWizardPageDataSource();

    bool isComplete() const;
    QStringList dataSourceFieldList() const;

    DataSourcePtr dataSource() const;

  public Q_SLOTS:
    void sourceChanged(const QString&);
    void configureSource();
    void sourceValid(QString filename, int requestID);
    void updateTypeActivated(int);

  Q_SIGNALS:
    void dataSourceChanged();

  private:
    bool _pageValid;
    ObjectStore *_store;
    DataSourcePtr _dataSource;
    int _requestID;
    void updateUpdateBox();
};

class DataWizardPageVectors : public QWizardPage, Ui::DataWizardPageVectors
{
  Q_OBJECT
  public:
    DataWizardPageVectors(QWidget *parent);
    virtual ~DataWizardPageVectors();

    bool isComplete() const;
    QListWidget* plotVectors() const;

  public Q_SLOTS:
    void add();
    void remove();
    void up();
    void down();
    void filterVectors(const QString&);
    void searchVectors();
    void updateVectors();

  private:
    bool vectorsSelected() const;

};

class DataWizardPageFilters : public QWizardPage, Ui::DataWizardPageFilters
{
  Q_OBJECT
  public:
    DataWizardPageFilters(QWidget *parent);
    virtual ~DataWizardPageFilters();

};

class DataWizardPagePlot : public QWizardPage, Ui::DataWizardPagePlot
{
  Q_OBJECT
  public:
    enum CurvePlotPlacement { OnePlot, MultiplePlots, CyclePlotCount, CycleExisting, ExistingPlot };

    enum PlotTabPlacement { CurrentTab, NewTab, SeparateTabs };

    DataWizardPagePlot(QWidget *parent);
    virtual ~DataWizardPagePlot();

    CurvePlacement::Layout layout() const;
    int gridColumns() const;

    bool drawLines() const;
    bool drawPoints() const;
    bool drawLinesAndPoints() const;

    bool PSDLogX() const;
    bool PSDLogY() const;

    bool legendsOn() const;
    bool legendsAuto() const;
    bool legendsVertical() const;

    bool rescaleFonts() const;
    bool shareAxis() const;

    CurvePlotPlacement curvePlacement() const;
    PlotItemInterface *existingPlot() const;

    PlotTabPlacement plotTabPlacement() const;

    int plotCount() const;

  public Q_SLOTS:
    void updatePlotBox();
    void updateButtons();
};

class DataWizardPageDataPresentation : public QWizardPage, Ui::DataWizardPageDataPresentation
{
  Q_OBJECT
  public:
    DataWizardPageDataPresentation(ObjectStore *store, QWidget *parent);
    virtual ~DataWizardPageDataPresentation();

    int nextId() const;
    bool isComplete() const;

    bool createXAxisFromField() const;
    QString vectorField() const;

    bool plotPSD() const;
    bool plotData() const;
    bool plotDataPSD() const;

    VectorPtr selectedVector() const;

    FFTOptions* getFFTOptions() const;
    DataRange* dataRange() const;

  public Q_SLOTS:
    void applyFilter(bool);
    void updateVectors();
    void optionsUpdated();

  Q_SIGNALS:
    void filterApplied(bool);

  private:
    bool _pageValid;
    bool validOptions();
};

class DataWizard : public QWizard
{
  Q_OBJECT
  public:
    enum DataWizardPages {PageDataSource, PageVectors, PageDataPresentation, PageFilters, PagePlot};

    DataWizard(QWidget *parent, const QString& fn = QString());
    virtual ~DataWizard();

    QStringList dataSourceFieldList() const;
    QStringList dataSourceIndexList() const;

  private:
    DataWizardPageDataSource *_pageDataSource;
    DataWizardPageVectors *_pageVectors;
    DataWizardPageFilters *_pageFilters;
    DataWizardPagePlot *_pagePlot;
    DataWizardPageDataPresentation *_pageDataPresentation;

  Q_SIGNALS:
    void dataSourceLoaded(const QString&);

  private Q_SLOTS:
    void finished();

  private:
    Document *_document;
};

}
#endif

// vim: ts=2 sw=2 et
