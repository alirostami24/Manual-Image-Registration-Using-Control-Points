#ifndef DIALOG_H
#define DIALOG_H

#include <QDialog>

namespace Ui {
class CDialog;
}

class CDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CDialog(QWidget *parent = 0);
    ~CDialog();

private:
    Ui::CDialog *ui;
};

#endif // DIALOG_H
