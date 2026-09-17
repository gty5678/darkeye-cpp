#pragma once

#include <QDateTimeEdit>
#include <QKeySequenceEdit>
#include <QListView>
#include <QListWidget>
#include <QTableView>
#include <QTableWidget>
#include <QTreeView>

namespace darkeye {

class TokenTableView final : public QTableView
{
public:
    explicit TokenTableView(QWidget *parent = nullptr);
};

class TokenTableWidget final : public QTableWidget
{
public:
    explicit TokenTableWidget(QWidget *parent = nullptr);
    TokenTableWidget(int rows, int columns, QWidget *parent = nullptr);
};

class TreeView : public QTreeView
{
public:
    explicit TreeView(QWidget *parent = nullptr);
};
using TokenTreeView = TreeView;

class TokenListView final : public QListView
{
public:
    explicit TokenListView(QWidget *parent = nullptr);
};

class TokenListWidget final : public QListWidget
{
public:
    explicit TokenListWidget(QWidget *parent = nullptr);
};

class TokenDateTimeEdit final : public QDateTimeEdit
{
public:
    explicit TokenDateTimeEdit(QWidget *parent = nullptr);
};

class TokenKeySequenceEdit final : public QKeySequenceEdit
{
public:
    explicit TokenKeySequenceEdit(QWidget *parent = nullptr);
};

} // namespace darkeye
