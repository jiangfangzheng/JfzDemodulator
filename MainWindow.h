#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#if _MSC_VER >= 1600
	   #pragma execution_character_set("utf-8")
#endif

#include <QMainWindow>
#include <QtNetwork>
#include <QSerialPort>
#include <QSerialPortInfo>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = 0);
	~MainWindow();

private:
	// 时间
	QDateTime currentTime;
	// UDP相关-做客户端（从解调仪获取数据）
	QUdpSocket *receiver;	// UDP 接收
	int cnt;				// 从UDP接收计数
	bool alreadyOpenUDP;	// 已经开了UDP
	QByteArray datagramAll; // 完整的一个协议包
	double out[32][30];		// 存解析好的波长

	// TCP相关-做客户端（从解调仪获取数据）
	QTcpSocket *client;		// 转发的TCP客户端
	QString sendMsg;		// 发送给服务器的数据
	bool enableSentToClient;// 是否可以发给客户端了
	bool needCloseTcpClient;// 手动需要关闭TCP客户端

	// 串口相关
	int sendRow;
	int sendCol;
	int sleepCnt;
	bool needSleep;
	QSerialPort serial;
	bool openSerial;
	void initSeialPort();

private slots:
	void showMsg(QString msg);  // 显示信息
	void connectedMsg();        // UDP解调仪连接信息
	void disconnectedMsg();     // UDP解调仪断开信息
	void readPendingDatagrams();// UDP解调仪收到包时处理
	void clientOnconnect();     // TCP客户端连上
	void clientOnDisconnected();// TCP客户端断开
	void sentDataToTcpServer(); // 发送数据到TCP服务器
	void sentDataToTcpSerial(); // 发送数据到串口
	void serialRead();          // 串口收到数据
	void timerUpDate();         // 显示更新时间


	void on_pushButton_clicked();

	void on_pushButton_2_clicked();

	void on_pushButton_3_clicked();

	void on_pushButton_4_clicked();

private:
	Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
