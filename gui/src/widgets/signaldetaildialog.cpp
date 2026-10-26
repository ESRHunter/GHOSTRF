#include "signaldetaildialog.h"
#include "waveformwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTextEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QRegularExpression>

SignalDetailDialog::SignalDetailDialog(QWidget *parent, const SignalItem &item, bool isNew)
    : QDialog(parent), m_item(item), m_isNew(isNew)
{
    setWindowTitle(isNew ? "Добавить новый сигнал" : QString("Параметры сигнала: %1").arg(item.name));
    resize(640, 680);
    setupUi();
    updatePreview();
}

void SignalDetailDialog::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // Основные свойства
    auto *formGroup = new QGroupBox("Параметры сигнала", this);
    auto *form = new QFormLayout(formGroup);
    form->setLabelAlignment(Qt::AlignRight);

    m_nameEdit = new QLineEdit(m_item.name.isEmpty() ? "New Signal" : m_item.name, this);
    form->addRow("Имя сигнала:", m_nameEdit);

    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem("Фиксированный код (Fixed)", SIG_KIND_FIXED);
    m_typeCombo->addItem("RAW Импульсы", SIG_KIND_RAW);
    m_typeCombo->addItem("KeeLoq брелок", SIG_KIND_KLQ_GEN);
    m_typeCombo->addItem("EV1527 Smart Remote", SIG_KIND_SMART_EV1527);
    m_typeCombo->setCurrentIndex(m_typeCombo->findData(m_item.type));
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SignalDetailDialog::onTypeChanged);
    form->addRow("Тип радиосигнала:", m_typeCombo);

    m_freqSpin = new QDoubleSpinBox(this);
    m_freqSpin->setRange(300.0, 928.0);
    m_freqSpin->setDecimals(3);
    m_freqSpin->setSingleStep(0.025);
    m_freqSpin->setSuffix(" МГц");
    m_freqSpin->setValue(m_item.freq > 0 ? m_item.freq : 433.92);
    connect(m_freqSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &SignalDetailDialog::updatePreview);
    form->addRow("Частота (CC1101):", m_freqSpin);

    m_protoCombo = new QComboBox(this);
    m_protoCombo->addItems({"EV1527", "PT2262", "Came 12bit", "Nice 12bit", "RAW", "KeeLoq"});
    m_protoCombo->setCurrentText(m_item.protoName.isEmpty() ? "EV1527" : m_item.protoName);
    connect(m_protoCombo, &QComboBox::currentTextChanged, this, &SignalDetailDialog::updatePreview);
    form->addRow("Протокол кодирования:", m_protoCombo);

    // HEX и DEC синхронизация
    auto *codeLayout = new QHBoxLayout();
    m_hexEdit = new QLineEdit(m_item.codeHex(), this);
    connect(m_hexEdit, &QLineEdit::textChanged, this, &SignalDetailDialog::onHexChanged);

    m_decSpin = new QSpinBox(this);
    m_decSpin->setRange(0, 0x7FFFFFFF);
    m_decSpin->setValue(static_cast<int>(m_item.code));
    connect(m_decSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &SignalDetailDialog::onDecChanged);

    codeLayout->addWidget(new QLabel("HEX:"));
    codeLayout->addWidget(m_hexEdit);
    codeLayout->addWidget(new QLabel("DEC:"));
    codeLayout->addWidget(m_decSpin);
    form->addRow("Код (HEX / DEC):", codeLayout);

    // Битность и Длина импульса
    auto *timingLayout = new QHBoxLayout();
    m_bitLenSpin = new QSpinBox(this);
    m_bitLenSpin->setRange(1, 64);
    m_bitLenSpin->setValue(m_item.bitLen > 0 ? m_item.bitLen : 24);
    connect(m_bitLenSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &SignalDetailDialog::updatePreview);

    m_pulseLenSpin = new QSpinBox(this);
    m_pulseLenSpin->setRange(50, 5000);
    m_pulseLenSpin->setSuffix(" µs");
    m_pulseLenSpin->setValue(m_item.pulseLen > 0 ? m_item.pulseLen : 320);
    connect(m_pulseLenSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &SignalDetailDialog::updatePreview);

    m_repeatsSpin = new QSpinBox(this);
    m_repeatsSpin->setRange(1, 20);
    m_repeatsSpin->setValue(m_item.repeats > 0 ? m_item.repeats : 5);

    timingLayout->addWidget(new QLabel("Бит:"));
    timingLayout->addWidget(m_bitLenSpin);
    timingLayout->addWidget(new QLabel("Базовый импульс (TE):"));
    timingLayout->addWidget(m_pulseLenSpin);
    timingLayout->addWidget(new QLabel("Повторов:"));
    timingLayout->addWidget(m_repeatsSpin);
    form->addRow("Тайминги:", timingLayout);

    mainLayout->addWidget(formGroup);

    // Блок RAW импульсов
    m_rawContainer = new QGroupBox("RAW последовательность импульсов (микросекунды)", this);
    auto *rawLayout = new QVBoxLayout(m_rawContainer);
    m_rawTextEdit = new QTextEdit(this);
    m_rawTextEdit->setPlaceholderText("Пример: 320, 960, 960, 320, 320, 960... (чередование HIGH и LOW импульсов)");
    QStringList rawStrs;
    for (uint16_t p : m_item.pulses) {
        rawStrs.append(QString::number(p));
    }
    m_rawTextEdit->setPlainText(rawStrs.join(", "));
    connect(m_rawTextEdit, &QTextEdit::textChanged, this, &SignalDetailDialog::onRawTextChanged);
    rawLayout->addWidget(m_rawTextEdit);
    mainLayout->addWidget(m_rawContainer);

    // Блок KeeLoq
    m_klqContainer = new QGroupBox("Данные KeeLoq брелока", this);
    auto *klqLayout = new QFormLayout(m_klqContainer);
    m_klqSerialEdit = new QLineEdit(m_item.keeloqSerialHex(), this);
    m_klqBtnSpin = new QSpinBox(this);
    m_klqBtnSpin->setRange(0, 15);
    m_klqBtnSpin->setValue(m_item.keeloqBtn);
    m_klqEncEdit = new QLineEdit(m_item.keeloqEncHex(), this);
    klqLayout->addRow("Серийный номер (28-bit):", m_klqSerialEdit);
    klqLayout->addRow("Код кнопки:", m_klqBtnSpin);
    klqLayout->addRow("Зашифрованный счетчик (32-bit):", m_klqEncEdit);
    mainLayout->addWidget(m_klqContainer);

    // Осциллограф предварительного просмотра
    auto *oscGroup = new QGroupBox("Интерактивная осциллограмма", this);
    auto *oscLayout = new QVBoxLayout(oscGroup);
    m_waveform = new WaveformWidget(this);
    m_waveform->setMinimumHeight(120);
    oscLayout->addWidget(m_waveform);
    mainLayout->addWidget(oscGroup);

    // Кнопки ОК / Отмена
    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto *btnCancel = new QPushButton("[ ОТМЕНА ]", this);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(btnCancel);

    auto *btnOk = new QPushButton(m_isNew ? "[ СОЗДАТЬ ]" : "[ ПРИМЕНИТЬ ]", this);
    btnOk->setObjectName("btnPrimary");
    connect(btnOk, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(btnOk);

    mainLayout->addLayout(btnLayout);

    onTypeChanged(m_typeCombo->currentIndex());
}

void SignalDetailDialog::onTypeChanged(int) {
    int curType = m_typeCombo->currentData().toInt();
    bool isRaw = (curType == SIG_KIND_RAW);
    bool isKlq = (curType == SIG_KIND_KLQ_GEN || m_item.isKeeloq);

    m_rawContainer->setVisible(isRaw);
    m_klqContainer->setVisible(isKlq);
    updatePreview();
}

void SignalDetailDialog::onHexChanged(const QString &text) {
    if (m_updatingCode) return;
    m_updatingCode = true;
    QString clean = text;
    clean.remove("0x", Qt::CaseInsensitive).remove(' ');
    bool ok = false;
    quint32 val = static_cast<quint32>(clean.toULongLong(&ok, 16));
    if (ok) {
        m_decSpin->setValue(static_cast<int>(val));
        m_item.code = val;
        updatePreview();
    }
    m_updatingCode = false;
}

void SignalDetailDialog::onDecChanged(int value) {
    if (m_updatingCode) return;
    m_updatingCode = true;
    m_item.code = static_cast<quint32>(value);
    m_hexEdit->setText(m_item.codeHex());
    updatePreview();
    m_updatingCode = false;
}

void SignalDetailDialog::onRawTextChanged() {
    QString text = m_rawTextEdit->toPlainText();
    QStringList tokens = text.split(QRegularExpression("[^0-9]+"), Qt::SkipEmptyParts);
    QVector<uint16_t> parsed;
    for (const QString &t : tokens) {
        int v = t.toInt();
        if (v > 0) parsed.append(static_cast<uint16_t>(v));
    }
    m_item.pulses = parsed;
    m_waveform->setPulses(m_item.pulses);
}

void SignalDetailDialog::updatePreview() {
    SignalItem temp = getSignal();
    QVector<uint16_t> wave = temp.toWaveformPulses();
    m_waveform->setPulses(wave);
}

SignalItem SignalDetailDialog::getSignal() const {
    SignalItem res = m_item;
    res.name = m_nameEdit->text().trimmed();
    if (res.name.isEmpty()) res.name = "Signal";
    res.type = m_typeCombo->currentData().toInt();
    res.freq = m_freqSpin->value();
    res.protoName = m_protoCombo->currentText();
    res.code = static_cast<quint32>(m_decSpin->value());
    res.bitLen = m_bitLenSpin->value();
    res.pulseLen = m_pulseLenSpin->value();
    res.repeats = m_repeatsSpin->value();

    if (res.type == SIG_KIND_RAW) {
        QString text = m_rawTextEdit->toPlainText();
        QStringList tokens = text.split(QRegularExpression("[^0-9]+"), Qt::SkipEmptyParts);
        res.pulses.clear();
        for (const QString &t : tokens) {
            int v = t.toInt();
            if (v > 0) res.pulses.append(static_cast<uint16_t>(v));
        }
        res.rawCount = res.pulses.size();
    }

    if (m_klqContainer->isVisible()) {
        bool ok = false;
        QString sText = m_klqSerialEdit->text().remove("0x", Qt::CaseInsensitive);
        res.keeloqSerial = static_cast<quint32>(sText.toULongLong(&ok, 16));
        res.keeloqBtn = m_klqBtnSpin->value();
        QString eText = m_klqEncEdit->text().remove("0x", Qt::CaseInsensitive);
        res.keeloqEnc = static_cast<quint32>(eText.toULongLong(&ok, 16));
        res.isKeeloq = true;
    }

    return res;
}
