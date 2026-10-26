#pragma once

#include <QDialog>
#include "../models/signalitem.h"

class QLineEdit;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QTextEdit;
class WaveformWidget;

class SignalDetailDialog : public QDialog {
    Q_OBJECT
public:
    explicit SignalDetailDialog(QWidget *parent = nullptr, const SignalItem &item = SignalItem(), bool isNew = false);

    SignalItem getSignal() const;

private slots:
    void onTypeChanged(int index);
    void onHexChanged(const QString &text);
    void onDecChanged(int value);
    void onRawTextChanged();
    void updatePreview();

private:
    SignalItem m_item;
    bool m_isNew;
    bool m_updatingCode = false;

    QLineEdit *m_nameEdit;
    QComboBox *m_typeCombo;
    QDoubleSpinBox *m_freqSpin;
    QComboBox *m_protoCombo;
    QLineEdit *m_hexEdit;
    QSpinBox *m_decSpin;
    QSpinBox *m_bitLenSpin;
    QSpinBox *m_pulseLenSpin;
    QSpinBox *m_repeatsSpin;

    // RAW поля
    QWidget *m_rawContainer;
    QTextEdit *m_rawTextEdit;

    // KeeLoq поля
    QWidget *m_klqContainer;
    QLineEdit *m_klqSerialEdit;
    QSpinBox *m_klqBtnSpin;
    QLineEdit *m_klqEncEdit;

    WaveformWidget *m_waveform;

    void setupUi();
};
