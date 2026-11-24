/*
 *  This file is part of the caQtDM Framework, developed at the Paul Scherrer Institut,
 *  Villigen, Switzerland
 *
 *  The caQtDM Framework is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  The caQtDM Framework is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with the caQtDM Framework.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Copyright (c) 2010 - 2026
 *
 *  Author:
 *    Erik Schwarz
 *  Contact details:
 *    erik.schwarz@psi.ch
 */

#ifndef FAKEFORMWINDOW_H
#define FAKEFORMWINDOW_H

#include <QDesignerFormWindowCursorInterface>
#include <QDesignerFormWindowInterface>
#include <QDir>
#include <QMap>
#include <QString>
#include <QVariant>

class FakeCursor : public QDesignerFormWindowCursorInterface
{
public:
    void setProperty(const QString &name, const QVariant &value) override
    {
        properties[name] = value;
    }

    QDesignerFormWindowInterface *formWindow() const override { return Q_NULLPTR; }
    bool hasSelection() const override { return false; }
    int selectedWidgetCount() const override { return 0; }
    QWidget *selectedWidget(int) const override { return Q_NULLPTR; }
    int widgetCount() const override { return 0; }
    QWidget *widget(int) const override { return Q_NULLPTR; }
    QWidget *current() const override { return Q_NULLPTR; }
    void setWidgetProperty(QWidget *widget, const QString &name, const QVariant &value) override
    {
        propertyWidget = widget;
        widgetProperties[name] = value;
    }
    void resetWidgetProperty(QWidget *, const QString &) override {}
    int position() const override { return 0; }

    void setPosition(int, MoveMode) override {}
    bool movePosition(MoveOperation, MoveMode) override { return false; }

    QMap<QString, QVariant> properties;
    QWidget *propertyWidget = nullptr;
    QMap<QString, QVariant> widgetProperties;
};

class FakeFormWindow : public QDesignerFormWindowInterface
{
    Q_OBJECT
public:
    explicit FakeFormWindow(QWidget *parent = Q_NULLPTR)
        : QDesignerFormWindowInterface(parent)
    {}

    FakeCursor *fakeCursor() { return &m_cursor; }

    QDesignerFormWindowCursorInterface *cursor() const override { return &m_cursor; }

    QString fileName() const override { return {}; }
    QDir absoluteDir() const override { return {}; }
    QString contents() const override { return {}; }
    QStringList checkContents() const override { return {}; }
    bool setContents(QIODevice *, QString *) override { return false; }
    bool setContents(const QString &) override { return false; }
    QWidget *mainContainer() const override { return Q_NULLPTR; }
    void setMainContainer(QWidget *) override {}
    bool isManaged(QWidget *) const override { return false; }
    void manageWidget(QWidget *) override {}
    void unmanageWidget(QWidget *) override {}
    QUndoStack *commandHistory() const override { return Q_NULLPTR; }
    void beginCommand(const QString &) override {}
    void endCommand() override {}
    void simplifySelection(QWidgetList *) const override {}
    void emitSelectionChanged() override {}
    bool isDirty() const override { return false; }
    Feature features() const override { return Feature(0); }
    bool hasFeature(Feature) const override { return false; }
    void setFeatures(Feature) override {}
    QString author() const override { return {}; }
    void setAuthor(const QString &) override {}
    QString comment() const override { return {}; }
    void setComment(const QString &) override {}
    void layoutDefault(int *, int *) override {}
    void setLayoutDefault(int, int) override {}
    void layoutFunction(QString *, QString *) override {}
    void setLayoutFunction(const QString &, const QString &) override {}
    QString pixmapFunction() const override { return {}; }
    void setPixmapFunction(const QString &) override {}
    QString exportMacro() const override { return {}; }
    void setExportMacro(const QString &) override {}
    QStringList includeHints() const override { return {}; }
    void setIncludeHints(const QStringList &) override {}
    ResourceFileSaveMode resourceFileSaveMode() const override
    {
        return ResourceFileSaveMode::DontSaveResourceFiles;
    }
    void setResourceFileSaveMode(ResourceFileSaveMode) override{};
    QtResourceSet *resourceSet() const override { return Q_NULLPTR; }
    void setResourceSet(QtResourceSet *) override {};
    int toolCount() const override { return 0; }
    int currentTool() const override { return 0; }
    void setCurrentTool(int) override {};
    QDesignerFormWindowToolInterface *tool(int) const override { return Q_NULLPTR; }
    void registerTool(QDesignerFormWindowToolInterface *) override {}
    QPoint grid() const override { return QPoint(0, 0); }
    QWidget *formContainer() const override { return Q_NULLPTR; }
    QStringList resourceFiles() const override { return {}; }
    void addResourceFile(const QString &) override {}
    void removeResourceFile(const QString &) override {}
    void ensureUniqueObjectName(QObject *) override {}
    void setGrid(const QPoint &) override {}
    void setFileName(const QString &) override {}
    void editWidgets() override {}
    void setDirty(bool) override {}
    void clearSelection(bool = true) override {}
    void selectWidget(QWidget *, bool = true) override {}

private:
    mutable FakeCursor m_cursor;
};

#endif // FAKEFORMWINDOW_H
