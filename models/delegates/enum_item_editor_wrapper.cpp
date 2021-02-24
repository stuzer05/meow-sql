#include "enum_item_editor_wrapper.h"
#include "helpers/parsing.h"
#include "helpers/formatting.h"
#include <QDateTimeEdit>
#include <QComboBox>
#include "app/app.h"
#include <QDebug>

namespace meow {
namespace models {
namespace delegates {

QWidget * EnumItemEditorWrapper::createEditor(QWidget *parent,
                              const QStyleOptionViewItem &option,
                              const QModelIndex &index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);


    _editor = new QComboBox(parent);

    return _editor;
}

void EnumItemEditorWrapper::setEditorData(QWidget *editor,
                                              const QModelIndex &index) const
{
    auto comboBox = static_cast<QComboBox *>(editor);
    // auto model = static_cast<const models::forms::TableForeignKeysModel *>(
    //         index.model()); // TODO model for enum

    // comboBox->addItems(model->referenceTables()); // TODO fill from enum model

    QStringList t = {"regular", "preorder"};
    comboBox->addItems(t);

    QString value = index.model()->data(index, Qt::EditRole).toString();
    comboBox->setCurrentIndex(comboBox->findText(value));
}

void EnumItemEditorWrapper::setModelData(QWidget *editor,
                                             QAbstractItemModel *model,
                                             const QModelIndex &index) const
{
    auto comboBox = static_cast<QComboBox *>(editor);
    QVariant curData = comboBox->currentText();
    model->setData(index, curData, Qt::EditRole);
}

void EnumItemEditorWrapper::updateEditorGeometry(
        QWidget *editor,
        const QStyleOptionViewItem &option,
        const QModelIndex &index) const
{
    Q_UNUSED(index);
    editor->setGeometry(option.rect);
}

} // namespace delegates
} // namespace models
} // namespace meow
