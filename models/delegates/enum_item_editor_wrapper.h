#ifndef ENUM_ITEM_EDITOR_WRAPPER_H
#define ENUM_ITEM_EDITOR_WRAPPER_H

#include "edit_query_data_delegate.h"
#include <QStyledItemDelegate>

namespace meow {
namespace models {
namespace delegates {

class EnumItemEditorWrapper : public ItemEditorWrapper
{
public:
        EnumItemEditorWrapper(EditQueryDataDelegate * delegate)
        : ItemEditorWrapper(delegate) {

    }

    virtual QWidget * createEditor(QWidget *parent,
                           const QStyleOptionViewItem &option,
                           const QModelIndex &index) const override;

    virtual void setEditorData(QWidget *editor,
                               const QModelIndex &index) const override;

    virtual void setModelData(QWidget *editor,
                      QAbstractItemModel *model,
                      const QModelIndex &index) const override;

    void updateEditorGeometry(QWidget *editor,
                              const QStyleOptionViewItem &option,
                              const QModelIndex &index) const;
};

} // namespace delegates
} // namespace models
} // namespace meow

#endif // ENUM_ITEM_EDITOR_WRAPPER_H
