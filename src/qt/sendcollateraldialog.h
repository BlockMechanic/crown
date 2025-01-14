#ifndef SENDCOINSDIALOG_H
#define SENDCOINSDIALOG_H

#include <qt/sendcoinsdialog.h>

class SendCollateralDialog : public SendCoinsDialog
{
    Q_OBJECT

public:
    enum Node {
        SYSTEMNODE,
        MASTERNODE
    };
    explicit SendCollateralDialog(const PlatformStyle *platformStyle, Node node, QWidget *parent = 0);
    void send(QList<SendAssetsRecipient> &recipients);
    bool fAutoCreate;
private:
    bool instantXChecked();
    void processSendCoinsReturn(const WalletModel::SendCoinsReturn &sendCoinsReturn, const QString &msgArg = QString());
    Node node;
};

#endif // SENDCOINSDIALOG_H
