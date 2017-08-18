#include "MainWindow.h"
#include "ui_MainWindow.h"

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	ui(new Ui::MainWindow)
{
	ui->setupUi(this);
	// UDP客户端
	cnt = 0;				// 发送归零
	alreadyOpenUDP = false;	// 还没开UDP
	// 数组归零
	for(auto i=0; i< 32; ++i)
		for(auto j=0; j<30; ++j)
			out[i][j] = 0;
	// TCP客户端
	enableSentToClient =false;// 不能发给TCP服务器
	needCloseTcpClient =false;// 不需要关闭TCP连接
	// 定时器相关-TCP 5s一次
	QTimer *timer1 = new QTimer(this);
	connect(timer1,SIGNAL(timeout()),this,SLOT(sentDataToTcpServer()));
	timer1->start(5000);
	// 定时器相关-串口 10ms一次
	sendRow = 0;
	sendCol = 0;
	needSleep = false;
	sleepCnt = 0;
	QTimer *timer2 = new QTimer(this);
	connect(timer2,SIGNAL(timeout()),this,SLOT(sentDataToTcpSerial()));
	timer2->start(10);
	// 串口初始化
	initSeialPort();
	openSerial = false;
}

MainWindow::~MainWindow()
{
	serial.close();
	delete ui;
}

void MainWindow::showMsg(QString msg)
{
	ui->textEdit->setText(msg);
}

// 连接解调仪（打开UDP客户端）
void MainWindow::on_pushButton_clicked()
{
	// 如果还没打开UDP，就打开
	if(!alreadyOpenUDP)
	{
		// 建立UDP客户端
		receiver = new QUdpSocket(this);
		connect(receiver, SIGNAL(readyRead()),this, SLOT(readPendingDatagrams()));// 收到报文
		connect(receiver,SIGNAL(connected()),this,SLOT(connectedMsg()));          // 判断连上
		connect(receiver,SIGNAL(disconnected()),this,SLOT(disconnectedMsg()));    // 判断断开

		// 发‘0’激活
		QByteArray datagram;
		datagram.resize(1);
		datagram[0] = '0';
		QHostAddress serverAddress = QHostAddress("192.168.0.119");//向特定IP发送 192.168.0.119    127.0.0.1
		receiver->writeDatagram(datagram.data(), datagram.size(), serverAddress, 4010);
		alreadyOpenUDP = true; // 已经开启UDP客户端
	}
}

// 解调仪协议解析
bool InterrogatorToWave(QByteArray &datagram, double out[32][30])
{
	// 4个头应该为 1 12 1 14
//	unsigned char head1 = datagram[0];
//	unsigned char head2 = datagram[1];
//	unsigned char head3 = datagram[994];
//	unsigned char head4 = datagram[995];
//	qDebug()<< QString::number(head1) << QString::number(head2) <<QString::number(head3) <<QString::number(head4);
	unsigned char data1[16][62];
	unsigned char data2[16][62];
	for(auto row=0; row<16; ++row)
	{
		for(auto col=0; col<62; ++col)
		{
			data1[row][col] = datagram[row*62 + col + 2]; // 前16通道
		}
	}
	for(auto row=0; row<16; ++row)
	{
		for(auto col=0; col<62; ++col)
		{
			data2[row][col] = datagram[row*62 + col + 996]; // 后16通道
		}
	}
	double dataWave1[16][30];
	double dataWave2[16][30];
	for(auto row=0; row<16; ++row)
	{
//		qDebug()<< "第"<<QString::number(row+1)<<"行:";
		for(auto col=0; col<30; ++col)
		{
//			1520+（256×63+139）/1000=1536.267
			unsigned char temp1 = data1[row][col*2+2];
			unsigned char temp2 = data1[row][col*2+3];
			double value = 1520+(256*temp2 + temp1)*1.0/1000;
			if(value > 1525)
				dataWave1[row][col] = 1520+(256*temp2 + temp1)*1.0/1000; // 前16通道
			else
				dataWave1[row][col] = 0;
//			qDebug()<< dataWave1[row][col];
		}
	}
	for(auto row=0; row<16; ++row)
	{
//		qDebug()<< "第"<<QString::number(row+1+16)<<"行:";
		for(auto col=0; col<30; ++col)
		{
//			1520+（256×63+139）/1000=1536.267
			unsigned char temp1 = data2[row][col*2+2];
			unsigned char temp2 = data2[row][col*2+3];
			double value = 1520+(256*temp2 + temp1)*1.0/1000;
			if(value > 1525)
				dataWave2[row][col] = 1520+(256*temp2 + temp1)*1.0/1000; // 后16通道
			else
				dataWave2[row][col] = 0;
//			qDebug()<< dataWave2[row][col];
		}
	}
	for(auto row=0; row<16; ++row)
	{
		for(auto col=0; col<30; ++col)
		{
			out[row][col] = dataWave1[row][col];
		}
	}
	for(auto row=0; row<16; ++row)
	{
		for(auto col=0; col<30; ++col)
		{
			out[row + 16][col] = dataWave2[row][col];
		}
	}
	return false;
}

// UDP 连上
void MainWindow::connectedMsg()
{
	alreadyOpenUDP = true;
	qDebug()<<"连上";
	ui->textEdit->setText("连上");
}

// UDP 断开
void MainWindow::disconnectedMsg()
{
	alreadyOpenUDP = false;
	qDebug()<<"断开";
	ui->textEdit->setText("断开");
}

// UDP 收到数据后处理解析
void MainWindow::readPendingDatagrams()
{
	while (receiver->hasPendingDatagrams())
	{
		// 取直接收到的报文
		QByteArray datagram;
		datagram.resize(receiver->pendingDatagramSize());
		receiver->readDatagram(datagram.data(), datagram.size()); //数据接收在datagram里
		// 如果报文小于1000， 没收全，继续收，累计到datagramAll
		if(datagram.size() < 1000)
		{
			datagramAll.append(datagram);
			ui->label->setText("sandeepin poi poi poi!");
		}
		// 如果报文大于2000 或者 开始不是前16通道数据，舍弃合并的datagramAll
		if(datagramAll.size() > 2000 || datagramAll.at(1) != 12)
		{
			datagramAll.clear();
		}
		// 如果恰好收对了，开始协议解析
		if(datagramAll.size() == 1988 && datagramAll.at(1) == 12 && datagramAll.at(995) == 14 )
		{
			// 协议解析出波长
			InterrogatorToWave(datagramAll, out);
			// 用于前台显示
			QString temp;
			QString temp2;
			QString v;
			for(auto row=0; row<32; ++row)
			{
				temp2.sprintf("CH%02d ", row+1);
				for(auto col=0; col<16; ++col) // 人为最多显示16列
				{
					temp2 = temp2 + v.sprintf("%08.3lf", out[row][col]) + " ";
				}
				temp += temp2 + "\n";
			}
			ui->textEdit->setText("接收" + QString::number(cnt++) + "次，包大小 "+ QString::number(datagramAll.size()) +"，包内容：\n" + temp);
			datagramAll.clear();
		}
		// 如果计数太大，清空
		if(cnt > 100000000)
			cnt = 0;
	}
}

// 打开 TCP 客户端
void MainWindow::on_pushButton_2_clicked()
{
	// 如果不能发送到TCP客户端（TCP没开），则建立一个TCP
	if(!enableSentToClient)
	{
		client = new QTcpSocket(this);
		client->connectToHost(QHostAddress("127.0.0.1"), 10101);
		connect(client, SIGNAL(connected()), this, SLOT(clientOnconnect()));
		connect(client, SIGNAL(disconnected()), this, SLOT(clientOnDisconnected()));
	}
}

// 断开 TCP 客户端
void MainWindow::on_pushButton_3_clicked()
{
	// 手动需要断开
	needCloseTcpClient = true;
	// 没开才执行 断开TCP client
	if(enableSentToClient)
		client->disconnectFromHost();
}

// TCP客户端断开 槽
void MainWindow::clientOnDisconnected()
{
	enableSentToClient =false;
	qDebug() << "自己的client断开";
	ui->label_2->setText("自己的client断开");
	// 如果断开了，并且不是手动需要断开的，就重连
	if(!enableSentToClient && !needCloseTcpClient)
	{
		client->connectToHost(QHostAddress("127.0.0.1"), 10101);
		enableSentToClient = true;
	}
}

// TCP客户端连上 槽
void MainWindow::clientOnconnect()
{
	qDebug() << "自己的client连上";
	ui->label_2->setText("自己的client连上");
	// 可以发数据了
	enableSentToClient =true;
	// 不需要手动断开
	needCloseTcpClient = false;
}

 // 发送数据到TCP服务器-定时任务 5秒一次
void MainWindow::sentDataToTcpServer()
{
	// 能连上服务器后 发送到TCP服务端
	if(enableSentToClient)
	{
		sendMsg = "";
		QString CHxx_;
		QString CH_xx;
		QString wave;
		QString oneData;
		for(auto row=0; row<32; ++row)
		{
			CHxx_.sprintf("S%02d,", row);
			for(auto col=0; col<20; ++col) // 人为最多显示16列
			{
				CH_xx.sprintf("%02d,", col);
				wave.sprintf("%08.3lf\n", out[row][col]);
				oneData = CHxx_ + CH_xx + wave;
				sendMsg += oneData;
			}
		}
		client->write(sendMsg.toLatin1());
	}


}

// 发送数据到串口 -定时任务 100毫秒一次
void MainWindow::sentDataToTcpSerial()
{
	// 串口开了，发到串口上
//	qDebug()<<"openSerial"<<openSerial;

	// 如果需要睡眠，就睡1s
	if(needSleep)
	{
		sleepCnt ++;
		if(sleepCnt == 100)
		{
			sleepCnt = 0;
			needSleep = false;
		}
	}
	// 否则发数据
	else
	{
		if(openSerial)
		{
			sendMsg = "";
			QString CHxx_;
			QString CH_xx;
			QString wave;
			QString oneData;
			CHxx_.sprintf("S%02d,", sendRow);
			CH_xx.sprintf("%02d,", sendCol);
			wave.sprintf("%08.3lf\r\n", out[sendRow][sendCol]);
			oneData = CHxx_ + CH_xx + wave;
			serial.write(oneData.toLatin1());

			// 发到最后一条后睡眠
			if(sendRow == 31 && sendCol == 19)
				needSleep = true;

			// 每进定时器只发一条
			if(sendCol == 19)
			{
				sendCol = 0;
				if(sendRow == 31)
					sendRow = 0;
				else
					sendRow++; // 发送的行加一
			}
			else
				sendCol++;
		}
	}
}

// 初始化串口
void MainWindow::initSeialPort()
{
	connect(&serial, SIGNAL(readyRead()), this, SLOT(serialRead()));   //连接槽
	// 获取串口名字
	QList<QSerialPortInfo>  infos = QSerialPortInfo::availablePorts();
	if(infos.isEmpty())
	{
		ui->comboBox->addItem("无串口");
		return;
	}
//    ui->comboBox->addItem("串口");
	foreach (QSerialPortInfo info, infos) {
		ui->comboBox->addItem(info.portName());
	}
}

// 开启串口
void MainWindow::on_pushButton_4_clicked()
{
	// 已经开了，就关
	if(openSerial)
	{
		serial.close();
		ui->label_3->setText("[已关闭串口]");
		openSerial = false;
		ui->comboBox->setEnabled(true);
		ui->pushButton_4->setText("串口中转开启");
	}
	else
	{
		QString selectSerialName = ui->comboBox->currentText();
		QList<QSerialPortInfo> infos = QSerialPortInfo::availablePorts();
		QSerialPortInfo info;
		int i = 0;
		foreach(info, infos)
		{
			if(info.portName() == selectSerialName)
			{
				// 就是它
				break;
			}
			i++;
		}
		serial.setPort(info);
		serial.open(QIODevice::ReadWrite);         //读写打开
		ui->label_3->setText("[已开启串口]");
		openSerial = true;
		ui->comboBox->setEnabled(false);
		ui->pushButton_4->setText("串口中转关闭");

//		QByteArray res;
//		res.append(0x01);
//		res.append(0x02);
//		res.append(0x03);
//		res.append(0x04);
//		serial.write(res);
	}
}

void MainWindow::serialRead()
{
	ui->textEdit->append(serial.readAll());
}
