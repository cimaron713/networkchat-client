#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <Windowsx.h>
#include <stdlib.h>
#include <stdio.h>
#include <Commctrl.h>
#include <math.h>
#include "resources.h"

#define SERVERIPV4  "127.0.0.1"
#define DEFAULTID   "ID"
#define SERVERPORT  9000
#define BUFSIZE     256                          // ���� �޽��� ��ü ũ��
#define MSGSIZE     (BUFSIZE-sizeof(int)-IDSIZE) // ä�� �޽��� �ִ� ����
#define IDSIZE      20                           // �����ID ����

#define CHATTING        1000                     // �޽��� Ÿ��: ä��
#define DRAWLINE        1001                     // �޽��� Ÿ��: �� �׸���
#define DRAWSTRICTLINE  1002                     // �޽��� Ÿ��: ���� �׸���
#define DRAWTRI         1003                     // �޽��� Ÿ��: �ﰢ�� �׸���
#define DRAWRECT        1004                     // �޽��� Ÿ��: �簢�� �׸���
#define DRAWDIAMOND     1005                     // �޽��� Ÿ��: ������ �׸���
#define DRAWELLIPSE     1006                     // �޽��� Ÿ��: �� �׸���
#define DRAWPENTAGON    1007                     // �޽��� Ÿ��: ������ �׸���
#define DRAWHEXAGON     1008                     // �޽��� Ÿ��: ������ �׸���
#define DRAWHEART       1009                     // �޽��� Ÿ��: ��Ʈ �׸���
#define DRAWSTAR        1010                     // �޽��� Ÿ��: 5���� �׸���
#define DRAWCROSS       1011                     // �޽��� Ÿ��: ���ڰ� �׸���
#define DRAWMULTI       1012                     // �޽��� Ÿ��: ���ϱ� �׸���
#define DRAWRARROW      1013                     // �޽��� Ÿ��: ȭ��ǥ �׸���
#define DRAWCROWN       1014                     // �޽��� Ÿ��: �հ� �׸���
#define WM_DRAWIT   (WM_USER+1)                     // ����� ���� ������ �޽���

char* groupIP[] = { "235.7.8.9", "235.7.8.10", "235.7.8.11", "235.7.8.12", "��������" }; // ä�ù� �� ��������
int membershipCount = 0; // �׷� ����� ��
static SOCKET g_multicast_sock;

// ���� �޽��� ����
// sizeof(COMM_MSG) == 8
struct COMM_MSG {
	u_short type;
	u_short ip_addr_str;
	int  size; // �������� ������ ����
};

// ä�� �޽��� ����
// sizeof(CHAT_MSG) == 256
struct CHAT_MSG {
	int  type;
	char nickname[IDSIZE];
	char buf[MSGSIZE];
};

// �� �׸��� �޽��� ����
struct DRAWLINE_MSG {
	int  type;
	int linetype;
	int  color;
	int  x0, y0;
	int  x1, y1;
	int lineWidth; // �� ����
	BOOL isFill;
};

//�� ���� 
COLORREF Color = RGB(0, 0, 0);
int eraser_flag;

static struct ip_mreq mreq;

static HINSTANCE     g_hInst;            // ���� ���α׷� �ν��Ͻ� �ڵ�
static HWND          g_hDrawWnd;         // �׸��� �׸� ������
static HWND          g_hButtonSendMsg;      // '�޽��� ����' ��ư
static HWND          g_hEditStatus;         // ���� �޽��� ���
static HWND			 g_hButtonRemovemsg;	// 'ä�� ����'��ư
static char          g_ipaddr[64];         // ���� IP �ּ�
static u_short       g_port;            // ���� ��Ʈ ��ȣ
static HANDLE        g_hClientThread;      // ������ �ڵ�
static volatile BOOL g_bStart;            // ��� ���� ����
static SOCKET        g_sock;            // Ŭ���̾�Ʈ ����
static HANDLE        g_hReadEvent, g_hWriteEvent; // �̺�Ʈ �ڵ�
static CHAT_MSG      g_chatmsg;            // ä�� �޽��� ����
static DRAWLINE_MSG  g_drawmsg;            // �� �׸��� �޽��� ����
static DRAWLINE_MSG  pre_g_drawmsg;         // ���� ���� ����
static int           g_drawcolor;         // �� �׸��� ����
static int           g_drawlinetype;      // ���� Ÿ��
static COMM_MSG      g_drawsize;
static HWND          hGroupList;         // ������ multicast �׷� ���
static HWND          hAddToGroup;         // �׷� �����ϱ� ��ư
static HWND          hRemFromGroup;         // �׷� Ż��
static HWND          hGroupCombo;         // ä�ù�(��Ƽĳ��ƮIP) �׷� ����� ������ �ִ� combobox
static HWND          hEditUserID;         // ����� ID
static HWND          hEditIPaddr;
static HWND          hButtonConnect;      // ����, ���� ��ư
static HWND          hEditPort;

void add_membership(int index);      // combobox�� ���õ� multicast �׷� ����
void drop_membership(int index);   // combobox�� ���õ� multicast �׷� Ż��
void dropAllMulticastMemb();      // �������� ��� multicast �׷쿡�� Ż��

void enableConnectivity();         // ������ �ٽ� ���� �����ϰ� UI����
void disableConnectivity();         // ������ ���� ���ϰ� UI����
void enableSendButtonAfterJoiningGroup(); // �׷� ���� �� ���� �����ϰ� UI����
void disableSendButtonAfterJoiningGroup();


// ��ȭ���� ���ν���
BOOL CALLBACK DlgProc(HWND, UINT, WPARAM, LPARAM);
// ���� ��� ������ �Լ�
DWORD WINAPI ClientMain(LPVOID arg);
DWORD WINAPI ReadThread(LPVOID arg);
DWORD WINAPI ReceiveProcess(LPVOID arg); //Multicast �׷쿡�� ��� ������
DWORD WINAPI WriteThread(LPVOID arg);
// �ڽ� ������ ���ν���
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
// ���� ��Ʈ�� ��� �Լ�
void DisplayText(char* fmt, ...);
// ����� ���� ������ ���� �Լ�
int recvn(SOCKET s, char* buf, int len, int flags);
// ������ ��� �Լ�
int CalculateLog2(short num);
// ���� ��� �Լ�
void err_quit(char* msg);
void err_display(char* msg);

// ���� �Լ�
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	// ���� �ʱ�ȭ
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

	// �̺�Ʈ ����
	g_hReadEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
	if (g_hReadEvent == NULL) return 1;
	g_hWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (g_hWriteEvent == NULL) return 1;

	// ���� �ʱ�ȭ(�Ϻ�)
	g_chatmsg.type = CHATTING;
	g_drawmsg.type = DRAWLINE;
	g_drawmsg.color = Color;
	g_drawmsg.lineWidth = 1;
	g_drawsize.size = sizeof(DRAWLINE_MSG);

	// ��ȭ���� ����
	g_hInst = hInstance;
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DlgProc);

	// �̺�Ʈ ����
	CloseHandle(g_hReadEvent);
	CloseHandle(g_hWriteEvent);

	// ���� ����
	WSACleanup();
	return 0;
}

// ��ȭ���� ���ν���
BOOL CALLBACK DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static HWND hEditMsg;
	static HWND hColorButton;
	static HWND hShapeFill;
	static HWND hEraser;
	static HWND hShapeCombo;
	static HWND hLineWidthSlider;
	static HWND hDotLine;
	static HWND hSolidLine;
	static HWND hDashLine;
	static HWND hDashDot;

	CHOOSECOLOR COL;
	COLORREF crTemp[16];

	switch (uMsg) {
	case WM_INITDIALOG:
		// ��Ʈ�� �ڵ� ���
		hEditIPaddr = GetDlgItem(hDlg, IDC_IPADDR);
		hEditPort = GetDlgItem(hDlg, IDC_PORT);
		hButtonConnect = GetDlgItem(hDlg, IDC_CONNECT);
		g_hButtonSendMsg = GetDlgItem(hDlg, IDC_SENDMSG);
		g_hButtonRemovemsg = GetDlgItem(hDlg, IDC_RMV_LOG);
		hEditMsg = GetDlgItem(hDlg, IDC_MSG);
		g_hEditStatus = GetDlgItem(hDlg, IDC_STATUS);
		hShapeCombo = GetDlgItem(hDlg, IDC_COMBO_SHAPE);
		hEditUserID = GetDlgItem(hDlg, IDC_USERID_EDIT);
		hColorButton = GetDlgItem(hDlg, IDC_COLORBTN);
		hLineWidthSlider = GetDlgItem(hDlg, IDC_LINEWIDTH);
		hShapeFill = GetDlgItem(hDlg, IDC_ISFILL_CHECK);
		hEraser = GetDlgItem(hDlg, IDC_ERASER_BUTTON);
		hSolidLine = GetDlgItem(hDlg, IDC_SOLIDLINE);
		hDotLine = GetDlgItem(hDlg, IDC_DOTLINE);
		hDashLine = GetDlgItem(hDlg, IDC_DASHLINE);
		hDashDot = GetDlgItem(hDlg, IDC_DASHDOT);
		hAddToGroup = GetDlgItem(hDlg, IDC_ADD_MEMB);
		hRemFromGroup = GetDlgItem(hDlg, IDC_REM_MEMB);
		hGroupCombo = GetDlgItem(hDlg, IDC_GROUP_COMBO);
		hGroupList = GetDlgItem(hDlg, IDC_GROUP_LIST);

		// ��Ʈ�� �ʱ�ȭ
		SendMessage(hLineWidthSlider, TBM_SETPOS, TRUE, 1);
		SendMessage(hEditMsg, EM_SETLIMITTEXT, MSGSIZE, 0);
		SendMessage(hEditUserID, EM_SETLIMITTEXT, IDSIZE, 0);
		EnableWindow(g_hButtonSendMsg, FALSE);
		EnableWindow(g_hButtonRemovemsg, FALSE);
		SetDlgItemText(hDlg, IDC_IPADDR, (LPCSTR)SERVERIPV4);
		SetDlgItemText(hDlg, IDC_USERID_EDIT, (LPCSTR)DEFAULTID);
		SetDlgItemInt(hDlg, IDC_PORT, SERVERPORT, FALSE);
		SendMessage(hLineWidthSlider, TBM_SETRANGE, (WPARAM)1, (LPARAM)MAKELONG(1, 10));
		SendMessage(hShapeCombo, CB_ADDSTRING, 0, (LPARAM)"��");
		SendMessage(hShapeCombo, CB_ADDSTRING, 0, (LPARAM)"������");
		SendMessage(hShapeCombo, CB_ADDSTRING, 0, (LPARAM)"�ﰢ��");
		SendMessage(hShapeCombo, CB_ADDSTRING, 0, (LPARAM)"�簢��");
		SendMessage(hShapeCombo, CB_ADDSTRING, 0, (LPARAM)"������");
		SendMessage(hShapeCombo, CB_ADDSTRING, 0, (LPARAM)"����");
		SendMessage(hShapeCombo, CB_ADDSTRING, 0, (LPARAM)"������");
		SendMessage(hShapeCombo, CB_ADDSTRING, 0, (LPARAM)"������");
		SendMessage(hShapeCombo, CB_ADDSTRING, 0, (LPARAM)"��Ʈ");
		SendMessage(hShapeCombo, CB_ADDSTRING, 0, (LPARAM)"��");
		SendMessage(hShapeCombo, CB_ADDSTRING, 0, (LPARAM)"���ڰ�");
		SendMessage(hShapeCombo, CB_ADDSTRING, 0, (LPARAM)"���ϱ�");
		SendMessage(hShapeCombo, CB_ADDSTRING, 0, (LPARAM)"ȭ��ǥ");
		SendMessage(hShapeCombo, CB_ADDSTRING, 0, (LPARAM)"�հ�");
		SendMessage(hShapeCombo, CB_SETCURSEL, 0, (LPARAM)"��");
		SendMessage(hDotLine, BM_SETCHECK, BST_UNCHECKED, 0);
		SendMessage(hSolidLine, BM_SETCHECK, BST_CHECKED, 0);
		SendMessage(hDashLine, BM_SETCHECK, BST_UNCHECKED, 0);
		SendMessage(hDashDot, BM_SETCHECK, BST_UNCHECKED, 0);

		// �޺��ڽ� �ʱ�ȭ
		SendMessage(hGroupCombo, CB_ADDSTRING, 0, (LPARAM)"235.7.8.9");
		SendMessage(hGroupCombo, CB_ADDSTRING, 0, (LPARAM)"235.7.8.10");
		SendMessage(hGroupCombo, CB_ADDSTRING, 0, (LPARAM)"235.7.8.11");
		SendMessage(hGroupCombo, CB_ADDSTRING, 0, (LPARAM)"235.7.8.12");
		SendMessage(hGroupCombo, CB_SETCURSEL, 0, (LPARAM)"235.7.8.9");

		// ������ Ŭ���� ���
		WNDCLASS wndclass;
		wndclass.style = CS_HREDRAW | CS_VREDRAW;
		wndclass.lpfnWndProc = WndProc;
		wndclass.cbClsExtra = 0;
		wndclass.cbWndExtra = 0;
		wndclass.hInstance = g_hInst;
		wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
		wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
		wndclass.lpszMenuName = NULL;
		wndclass.lpszClassName = (LPCSTR)"MyWndClass";
		if (!RegisterClass(&wndclass)) return 1;

		// �ڽ� ������ ����
		g_hDrawWnd = CreateWindow((LPCSTR)"MyWndClass", (LPCSTR)"�׸� �׸� ������", WS_CHILD, 600, 100, 450, 450, hDlg, (HMENU)NULL, g_hInst, NULL);
		if (g_hDrawWnd == NULL) return 1;
		ShowWindow(g_hDrawWnd, SW_SHOW);
		UpdateWindow(g_hDrawWnd);

		return TRUE;

	case WM_HSCROLL:
		g_drawmsg.lineWidth = SendDlgItemMessage(hDlg, IDC_LINEWIDTH, TBM_GETPOS, 0, 0);
		return 0;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_CONNECT: {
			char buf[30];
			Button_GetText(hButtonConnect, buf, 30);
			if (strcmp(buf, "������ �����ϱ�") == 0) {
				GetDlgItemText(hDlg, IDC_IPADDR, (LPSTR)g_ipaddr, sizeof(g_ipaddr));
				g_port = GetDlgItemInt(hDlg, IDC_PORT, NULL, FALSE);
				GetDlgItemText(hDlg, IDC_USERID_EDIT, (LPSTR)g_chatmsg.nickname, IDSIZE);

				// ���� ��� ������ ����
				g_hClientThread = CreateThread(NULL, 0, ClientMain, NULL, 0, NULL);
				if (g_hClientThread == NULL) {
					MessageBox(hDlg, (LPCSTR)"Ŭ���̾�Ʈ�� ������ �� �����ϴ�."
						"\r\n���α׷��� �����մϴ�.", (LPCSTR)"����!", MB_ICONERROR);
					EndDialog(hDlg, 0);
				}
				else {
					Button_SetText(hButtonConnect, "���� ���� ����");
					while (g_bStart == FALSE); // ���� ���� ���� ��ٸ�
					disableConnectivity(); // ��ư Ȱ��ȭ/��Ȱ��ȭ
					SetFocus(hEditMsg);
				}
			}
			else {
				TerminateThread(g_hClientThread, 1);
				MessageBox(NULL, (LPCSTR)"������ ������ ����Ǿ����ϴ�.", (LPCSTR)"����!", MB_ICONINFORMATION);
				if (membershipCount != 0) {
					dropAllMulticastMemb(); // ������ ��� ���� ����
				}
				closesocket(g_sock);
				closesocket(g_multicast_sock);
				enableConnectivity(); // ��ư Ȱ��ȭ/��Ȱ��ȭ
			}
			return TRUE;
		}
		case IDC_SENDMSG:
			// �б� �ϷḦ ��ٸ�
			WaitForSingleObject(g_hReadEvent, INFINITE);
			GetDlgItemText(hDlg, IDC_MSG, (LPSTR)g_chatmsg.buf, MSGSIZE);
			// ���� �ϷḦ �˸�
			SetEvent(g_hWriteEvent);
			// �Էµ� �ؽ�Ʈ ��ü�� ���� ǥ��
			SendMessage(hEditMsg, EM_SETSEL, 0, -1);
			return TRUE;
		case IDCANCEL:
			if (MessageBox(hDlg, (LPCSTR)"������ �����Ͻðڽ��ϱ�?",
				(LPCSTR)"����", MB_YESNO | MB_ICONQUESTION) == IDYES)
			{
				if (membershipCount != 0) {
					dropAllMulticastMemb();
				}
				closesocket(g_sock);

				closesocket(g_multicast_sock);
				EndDialog(hDlg, IDCANCEL);
			}
			return TRUE;
		case IDC_RMV_LOG:
			SendMessage(g_hEditStatus, WM_SETTEXT, 0, (LPARAM)"");
			return TRUE;
		case IDC_COMBO_SHAPE:                     // ��� combobox ������ �� 
			switch (ComboBox_GetCurSel(hShapeCombo)) {
			case 0:   g_drawmsg.type = DRAWLINE; break;
			case 1:   g_drawmsg.type = DRAWSTRICTLINE; break;
			case 2:   g_drawmsg.type = DRAWTRI; break;
			case 3:   g_drawmsg.type = DRAWRECT; break;
			case 4:   g_drawmsg.type = DRAWDIAMOND; break;
			case 5:   g_drawmsg.type = DRAWELLIPSE; break;
			case 6:   g_drawmsg.type = DRAWPENTAGON; break;
			case 7:   g_drawmsg.type = DRAWHEXAGON; break;
			case 8:   g_drawmsg.type = DRAWHEART; break;
			case 9:  g_drawmsg.type = DRAWSTAR; break;
			case 10:  g_drawmsg.type = DRAWCROSS; break;
			case 11:  g_drawmsg.type = DRAWMULTI; break;
			case 12:  g_drawmsg.type = DRAWRARROW; break;
			case 13:  g_drawmsg.type = DRAWCROWN; break;

			default:
				g_drawmsg.type = DRAWLINE;
				break;
			}
		case IDC_ISFILL_CHECK:                     // ä��� checkbox
			g_drawmsg.isFill = SendMessage(hShapeFill, BM_GETCHECK, 0, 0);
			return TRUE;
		case IDC_ERASER_BUTTON:                     // ���찳 checkbox
			g_drawmsg.type = DRAWLINE;
			Color = RGB(255, 255, 255);
			return TRUE;
		case IDC_DOTLINE:
			g_drawmsg.linetype = PS_DOT;
			return TRUE;
		case IDC_SOLIDLINE:
			g_drawmsg.linetype = PS_SOLID;
			return TRUE;
		case IDC_DASHLINE:
			g_drawmsg.linetype = PS_DASH;
			return TRUE;
		case IDC_DASHDOT:
			g_drawmsg.linetype = PS_DASHDOT;
			return TRUE;
		case IDC_ALLCLEAR_BUTTON:   //��ü �����
		{
			g_hDrawWnd = CreateWindow((LPCSTR)"MyWndClass", (LPCSTR)"�׸� �׸� ������", WS_CHILD, 600, 100, 450, 450, hDlg, (HMENU)NULL, g_hInst, NULL);
			if (g_hDrawWnd == NULL) return 1;
			ShowWindow(g_hDrawWnd, SW_SHOW);
			UpdateWindow(g_hDrawWnd);
			return TRUE;
		}
		case IDC_COLORBTN: // �� ����
			memset(&COL, 0, sizeof(CHOOSECOLOR));
			COL.lStructSize = sizeof(CHOOSECOLOR);
			COL.hwndOwner = hDlg;
			COL.lpCustColors = crTemp;
			COL.Flags = 0;

			if (ChooseColor((LPCHOOSECOLORA)&COL) != 0) {
				Color = COL.rgbResult;
				InvalidateRect(hDlg, NULL, TRUE);
			}
			return TRUE;
		case IDC_ADD_MEMB: {   //�׷� ��� ���
			int combo_index = ComboBox_GetCurSel(hGroupCombo);
			int index = SendMessage(hGroupList, LB_FINDSTRINGEXACT, -1, (LPARAM)groupIP[combo_index]);
			if (index != LB_ERR) break;
			add_membership(combo_index);
			SendMessage(hGroupList, LB_ADDSTRING, 0, (LPARAM)groupIP[combo_index]);
			return TRUE;
		}
		case IDC_REM_MEMB: {
			// combox �ε��� ���ͼ�
			int combo_index = ListBox_GetCurSel(hGroupList);

			// ���õ� �׸��� ���ڿ� �������� -> buffer
			char buffer[20];
			SendMessage(hGroupList, LB_GETTEXT, combo_index, (LPARAM)buffer);
			// groupIp�迭���� ���õ� ���� �ε���
			for (int i = 0; i < 4; i++) {
				if (strcmp(groupIP[i], buffer) == 0) {
					drop_membership(i);      // ��Ƽĳ��Ʈ �׷쿡�� ����
					break;
				}
			}

			// delete in combobox
			SendMessage(hGroupList, LB_DELETESTRING, combo_index, 0);
			return TRUE;
		}
		}
		return FALSE;
	}
	return FALSE;
}

void dropAllMulticastMemb() {
	// �� ���� ���Ե� �׷��� �ҷ���
	// joinedGroups �� ���� -> �ε����� �����ϸ� �ɵ�(groupIp������ ���ε���)
	// �������� �ϳ��� ���ư��� drop_membership()
	int joinedGroupsIndex[4];

	// ���õ� ���� ���ͼ�
	int joinedGroupCounts = SendMessage(hGroupList, LB_GETCOUNT, (WPARAM)0, (LPARAM)0);
	for (int i = 0; i < joinedGroupCounts; i++) {
		for (int k = 0; k < 4; k++) {
			// ���õ� ���� ���� ���ͼ�
			char buf[20];
			SendMessage(hGroupList, LB_GETTEXT, i, (LPARAM)buf);
			// ������ ���� -> ��� ����ȭ�Ǳ� ������ ����
			if (strcmp(groupIP[k], buf) == 0) {
				joinedGroupsIndex[i] = k;
			}
		}
	}

	// �ϳ��� ����
	for (int i = joinedGroupCounts - 1; i >= 0; i--) {
		SendMessage(hGroupList, LB_DELETESTRING, i, 0);
		drop_membership(joinedGroupsIndex[i]);
	}
}

void add_membership(int index) {
	int retval;
	mreq.imr_multiaddr.s_addr = inet_addr(groupIP[index]);
	retval = setsockopt(g_multicast_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq));
	if (retval == SOCKET_ERROR) err_quit("setsockopt()");
	enableSendButtonAfterJoiningGroup();
	membershipCount += 1; // ��� �� ����
}

void drop_membership(int index) {
	int retval;
	mreq.imr_multiaddr.s_addr = inet_addr(groupIP[index]);
	retval = setsockopt(g_multicast_sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&mreq, sizeof(mreq));
	if (retval == SOCKET_ERROR) err_quit("setsockopt()");
	membershipCount -= 1; // ��� �� ����
}

// ���� ��� ������ �Լ�
DWORD WINAPI ClientMain(LPVOID arg) {
	int retval;

	mreq.imr_interface.s_addr = htonl(INADDR_ANY);

	g_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (g_sock == INVALID_SOCKET) err_quit("socket()");

	// connect()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = inet_addr(g_ipaddr);
	serveraddr.sin_port = htons(g_port);
	retval = connect(g_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR) err_quit("connect()");

	MessageBox(NULL, (LPCSTR)"������ �����߽��ϴ�.", (LPCSTR)"����!", MB_ICONINFORMATION);
	send(g_sock, g_chatmsg.nickname, 20, 0); // �г��� ����

	// �б� & ���� ������ ����
	HANDLE hThread[3];
	hThread[0] = CreateThread(NULL, 0, ReadThread, NULL, 0, NULL);
	hThread[1] = CreateThread(NULL, 0, WriteThread, NULL, 0, NULL);
	hThread[2] = CreateThread(NULL, 0, ReceiveProcess, NULL, 0, NULL);
	if (hThread[0] == NULL || hThread[1] == NULL || hThread[2] == NULL) {
		MessageBox(NULL, (LPCSTR)"�����带 ������ �� �����ϴ�."
			"\r\n���α׷��� �����մϴ�.",
			(LPCSTR)"����!", MB_ICONERROR);
		exit(1);
	}

	g_bStart = TRUE;

	// ������ ���� ���
	retval = WaitForMultipleObjects(3, hThread, FALSE, INFINITE);
	retval -= WAIT_OBJECT_0;
	if (retval == 0)
		TerminateThread(hThread[0], 1);
	else if (retval == 1)
		TerminateThread(hThread[1], 1);
	else
		TerminateThread(hThread[2], 1);
	CloseHandle(hThread[0]);
	CloseHandle(hThread[1]);
	CloseHandle(hThread[2]);

	g_bStart = FALSE;

	MessageBox(NULL, (LPCSTR)"������ ������ �������ϴ�", (LPCSTR)"�˸�", MB_ICONINFORMATION);
	EnableWindow(g_hButtonSendMsg, FALSE);
	EnableWindow(g_hButtonRemovemsg, FALSE);

	dropAllMulticastMemb();
	enableConnectivity();

	closesocket(g_multicast_sock);
	closesocket(g_sock);
	return 0;
}

// ������ �ޱ�
DWORD WINAPI ReadThread(LPVOID arg) {
	int retval;
	COMM_MSG comm_msg;
	CHAT_MSG chat_msg;
	DRAWLINE_MSG draw_msg;
	while (1) {
		retval = recvn(g_sock, (char*)&comm_msg, sizeof(COMM_MSG), 0);
		if (retval == 0 || retval == SOCKET_ERROR) {
			break;
		}
	}
	return 0;
}


DWORD WINAPI WriteThread(LPVOID arg) {
	int retval;
	COMM_MSG msg_size;
	// ������ ������ ���
	while (1) {
		// ���� �Ϸ� ��ٸ���
		WaitForSingleObject(g_hWriteEvent, INFINITE);

		// ���ڿ� ���̰� 0�̸� ������ ����
		if (strlen(g_chatmsg.buf) == 0) {
			// '�޽��� ����' ��ư Ȱ��ȭ
			EnableWindow(g_hButtonSendMsg, TRUE);
			EnableWindow(g_hButtonRemovemsg, TRUE);
			// �б� �Ϸ� �˸���
			SetEvent(g_hReadEvent);
			continue;
		}

		// �޼��� ����

		msg_size.size = strlen(g_chatmsg.buf) + 1 + IDSIZE + sizeof(int);
		msg_size.type = g_chatmsg.type;
		msg_size.ip_addr_str = 0;

		// ���õ� ��Ƽĳ��Ʈ �׷� �ε���
		int selectedGroupIndex = SendMessage(hGroupList, LB_GETCURSEL, (WPARAM)0, (LPARAM)0);

		// ���þȵǾ� ������ -1 �� ���ϵǱ� ������ 
		if (selectedGroupIndex != -1) {
			// ���õ� �ε����� �޺��ڽ� �׸��ؽ�Ʈ �ҷ����� -> buf
			char buf[20];
			SendMessage(hGroupList, LB_GETTEXT, selectedGroupIndex, (LPARAM)buf);
			for (int k = 0; k < 4; k++) {
				if (strcmp(groupIP[k], buf) == 0) {
					msg_size.ip_addr_str = (0x0001 << k);
				}
			}

			// ������ ũ�� ����
			retval = send(g_sock, (char*)&msg_size, sizeof(COMM_MSG), 0);
			if (retval == SOCKET_ERROR) {
				break;
			}

			// ������ ������
			retval = send(g_sock, (char*)&g_chatmsg, msg_size.size, 0);
			if (retval == SOCKET_ERROR) {
				break;
			}
		}

		// '�޽��� ����' ��ư Ȱ��ȭ
		EnableWindow(g_hButtonSendMsg, TRUE);
		EnableWindow(g_hButtonRemovemsg, TRUE);
		// �б� �Ϸ� �˸���
		SetEvent(g_hReadEvent);
	}
	return 0;
}

// �ڽ� ������ ���ν���
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	HDC hDC;
	int cx, cy;
	PAINTSTRUCT ps;
	RECT rect;
	HPEN hPen, hOldPen;
	static HBITMAP hBitmap;
	static HDC hDCMem;
	static int x0, y0;
	static int x1, y1;
	static BOOL bDrawing = FALSE;
	LOGBRUSH LogBrush;

	switch (uMsg) {
	case WM_CREATE:
		hDC = GetDC(hWnd);

		// ȭ���� ������ ��Ʈ�� ����
		cx = GetDeviceCaps(hDC, HORZRES);
		cy = GetDeviceCaps(hDC, VERTRES);
		hBitmap = CreateCompatibleBitmap(hDC, cx, cy);

		// �޸� DC ����
		hDCMem = CreateCompatibleDC(hDC);

		// ��Ʈ�� ���� �� �޸� DC ȭ���� ������� ĥ��
		SelectObject(hDCMem, hBitmap);
		SelectObject(hDCMem, GetStockObject(WHITE_BRUSH));
		SelectObject(hDCMem, GetStockObject(WHITE_PEN));
		Rectangle(hDCMem, 0, 0, cx, cy);

		ReleaseDC(hWnd, hDC);
		return 0;

	case WM_LBUTTONDOWN:
	{
		x0 = LOWORD(lParam);
		y0 = HIWORD(lParam);
		bDrawing = TRUE;
		g_drawsize.ip_addr_str = 0;

		// ���õ� ��Ƽĳ��Ʈ �׷� �ε���
		int selectedGroupIndex = SendMessage(hGroupList, LB_GETCURSEL, (WPARAM)0, (LPARAM)0);

		// ���þȵǾ� ������ -1 �� ���ϵǱ� ������ 
		if (selectedGroupIndex != -1) {
			// ���õ� �ε����� �޺��ڽ� �׸��ؽ�Ʈ �ҷ����� -> buf
			char buf[20];
			SendMessage(hGroupList, LB_GETTEXT, selectedGroupIndex, (LPARAM)buf);
			for (int k = 0; k < 4; k++) {
				if (strcmp(groupIP[k], buf) == 0) {
					g_drawsize.ip_addr_str = (0x0001 << k);
				}
			}
		}
		return 0;
	}
	case WM_MOUSEMOVE:
		// ���� ������ �ƴ� ���
		if (g_drawsize.ip_addr_str != 0) {
			if (bDrawing && g_bStart) {
				x1 = LOWORD(lParam);
				y1 = HIWORD(lParam);

				// �� �׸��� �޽��� ������
				g_drawmsg.x0 = x0;
				g_drawmsg.y0 = y0;
				g_drawmsg.x1 = x1;
				g_drawmsg.y1 = y1;

				if (g_drawmsg.type == DRAWLINE) {
					g_drawsize.type = DRAWLINE;
					g_drawmsg.color = Color;

					send(g_sock, (char*)&g_drawsize, sizeof(COMM_MSG), 0);
					send(g_sock, (char*)&g_drawmsg, g_drawsize.size, 0);
					x0 = x1;
					y0 = y1;
				}
			}
		}
		return 0;
	case WM_LBUTTONUP:
		// ���� ������ �ƴ� ���
		if (g_drawsize.ip_addr_str != 0) {
			if (g_drawmsg.type != DRAWLINE) {
				g_drawmsg.color = Color;
				g_drawsize.type = g_drawmsg.type;
				send(g_sock, (char*)&g_drawsize, sizeof(COMM_MSG), 0);
				send(g_sock, (char*)&g_drawmsg, g_drawsize.size, 0);
			}
			bDrawing = FALSE;
		}
		return 0;
	case WM_DRAWIT: {

		hDC = GetDC(hWnd);
		DRAWLINE_MSG* newLine = (DRAWLINE_MSG*)wParam;
		hPen = CreatePen(g_drawlinetype, newLine->lineWidth, g_drawcolor); // get lineWidth parameter

		// ȭ�鿡 �׸���
		hOldPen = (HPEN)SelectObject(hDC, hPen);    // change brush 
		hOldPen = (HPEN)SelectObject(hDCMem, hPen);
		switch (newLine->type) {
		case DRAWLINE:
			MoveToEx(hDC, newLine->x0, newLine->y0, NULL);
			LineTo(hDC, LOWORD(lParam), HIWORD(lParam));
			break;
		case DRAWTRI: {
			POINT trianglePoints[4] = {
			   { LOWORD(lParam), HIWORD(lParam) },
			   { newLine->x0, HIWORD(lParam) },
			   { (LOWORD(lParam) - newLine->x0) / 2 + newLine->x0, newLine->y0 },
			   { LOWORD(lParam), HIWORD(lParam) }
			};
			if (newLine->isFill) {
				LogBrush.lbColor = g_drawcolor;
				LogBrush.lbHatch = HS_VERTICAL;
				LogBrush.lbStyle = BS_SOLID;
				HBRUSH brush = CreateBrushIndirect(&LogBrush);
				SelectObject(hDC, brush);
			}
			else {
				SelectObject(hDC, GetStockObject(NULL_BRUSH));
			}

			Polygon(hDC, trianglePoints, 4);
			break;
		}
		case DRAWRECT:
			if (newLine->isFill) {
				LogBrush.lbColor = g_drawcolor;
				LogBrush.lbHatch = HS_VERTICAL;
				LogBrush.lbStyle = BS_SOLID;
				HBRUSH brush = CreateBrushIndirect(&LogBrush);
				SelectObject(hDC, brush);
			}
			else {
				SelectObject(hDC, GetStockObject(NULL_BRUSH));
			}
			Rectangle(hDC, newLine->x0, newLine->y0, LOWORD(lParam), HIWORD(lParam));
			break;
		case DRAWELLIPSE: {   //���϶�
			if (newLine->isFill) {
				LogBrush.lbColor = g_drawcolor;
				LogBrush.lbHatch = HS_VERTICAL;
				LogBrush.lbStyle = BS_SOLID;
				HBRUSH brush = CreateBrushIndirect(&LogBrush);
				SelectObject(hDC, brush);
			}
			else {
				SelectObject(hDC, GetStockObject(NULL_BRUSH));
			}

			Ellipse(hDC, newLine->x0, newLine->y0, LOWORD(lParam), HIWORD(lParam));
			SelectObject(hDC, hOldPen);
			break;
		}
		case DRAWSTRICTLINE: {
			MoveToEx(hDC, newLine->x0, newLine->y0, NULL);
			LineTo(hDC, newLine->x1, newLine->y1);
			break;
		}
		case DRAWPENTAGON: {
			POINT PolyPoints[6] = {
			   { newLine->x0, (newLine->y0 > HIWORD(lParam) ? (newLine->y0 - HIWORD(lParam)) / 3 + HIWORD(lParam) : (HIWORD(lParam) - newLine->y0) / 3 + newLine->y0) },
			   { ((LOWORD(lParam) - newLine->x0) * 2 / 9 + newLine->x0), (newLine->y0 > HIWORD(lParam) ? newLine->y0 : HIWORD(lParam)) },
			   { ((LOWORD(lParam) - newLine->x0) * 7 / 9 + newLine->x0), (newLine->y0 > HIWORD(lParam) ? newLine->y0 : HIWORD(lParam)) },
			   { LOWORD(lParam), (newLine->y0 > HIWORD(lParam) ? (newLine->y0 - HIWORD(lParam)) / 3 + HIWORD(lParam) : (HIWORD(lParam) - newLine->y0) / 3 + newLine->y0) },
			   { ((LOWORD(lParam) + newLine->x0) / 2), (newLine->y0 > HIWORD(lParam) ? HIWORD(lParam) : newLine->y0) },
			   { newLine->x0, (newLine->y0 > HIWORD(lParam) ? (newLine->y0 - HIWORD(lParam)) / 3 + HIWORD(lParam) : (HIWORD(lParam) - newLine->y0) / 3 + newLine->y0) }
			};

			if (newLine->isFill) {
				LogBrush.lbColor = g_drawcolor;
				LogBrush.lbHatch = HS_VERTICAL;
				LogBrush.lbStyle = BS_SOLID;
				HBRUSH brush = CreateBrushIndirect(&LogBrush);
				SelectObject(hDC, brush);
			}
			else {
				SelectObject(hDC, GetStockObject(NULL_BRUSH));
			}
			Polygon(hDC, PolyPoints, 6);
			break;
		}
		case DRAWSTAR: {
			POINT STARPoints[] = { {
			   (newLine->x0 + (LOWORD(lParam) - newLine->x0) / 2), newLine->y0 },
			{ (newLine->x0 + (LOWORD(lParam) - newLine->x0) * 3 / 8), (newLine->y0 + (HIWORD(lParam) - newLine->y0) * 3 / 8) },
			{ newLine->x0, (newLine->y0 + (HIWORD(lParam) - newLine->y0) * 3 / 8) },
			{ (newLine->x0 + (LOWORD(lParam) - newLine->x0) * 5 / 16), (HIWORD(lParam) - (HIWORD(lParam) - newLine->y0) * 3 / 8) },
			{ (newLine->x0 + (LOWORD(lParam) - newLine->x0) * 1 / 8), HIWORD(lParam) },
			{ (newLine->x0 + (LOWORD(lParam) - newLine->x0) / 2), (HIWORD(lParam) - (HIWORD(lParam) - newLine->y0) * 1 / 4) },
			{ (LOWORD(lParam) - (LOWORD(lParam) - newLine->x0) * 1 / 8), HIWORD(lParam) },
			{ (LOWORD(lParam) - (LOWORD(lParam) - newLine->x0) * 5 / 16), (HIWORD(lParam) - (HIWORD(lParam) - newLine->y0) * 3 / 8) },
			{ LOWORD(lParam), (newLine->y0 + (HIWORD(lParam) - newLine->y0) * 3 / 8) },
			{ (LOWORD(lParam) - (LOWORD(lParam) - newLine->x0) * 3 / 8), (newLine->y0 + (HIWORD(lParam) - newLine->y0) * 3 / 8) }
			};

			if (newLine->isFill) {
				LogBrush.lbColor = g_drawcolor;
				LogBrush.lbHatch = HS_VERTICAL;
				LogBrush.lbStyle = BS_SOLID;
				HBRUSH brush = CreateBrushIndirect(&LogBrush);
				SelectObject(hDC, brush);
			}
			else {
				SelectObject(hDC, GetStockObject(NULL_BRUSH));
			}
			Polygon(hDC, STARPoints, 10);
			break;
		}
		case DRAWHEXAGON: {
			POINT SixPoints[7] = {
			   {newLine->x0, (HIWORD(lParam) - newLine->y0) / 4 + newLine->y0},
			   {newLine->x0, (HIWORD(lParam) - newLine->y0) * 3 / 4 + newLine->y0, },
			   {(LOWORD(lParam) - newLine->x0) / 2 + newLine->x0, HIWORD(lParam)},
			   {LOWORD(lParam), (HIWORD(lParam) - newLine->y0) * 3 / 4 + newLine->y0  },
			   {LOWORD(lParam), (HIWORD(lParam) - newLine->y0) / 4 + newLine->y0  },
			   {(LOWORD(lParam) - newLine->x0) / 2 + newLine->x0,  newLine->y0   },
			   { newLine->x0, (HIWORD(lParam) - newLine->y0) / 4 + newLine->y0,}
			};

			if (newLine->isFill) {
				LogBrush.lbColor = g_drawcolor;
				LogBrush.lbHatch = HS_VERTICAL;
				LogBrush.lbStyle = BS_SOLID;
				HBRUSH brush = CreateBrushIndirect(&LogBrush);
				SelectObject(hDC, brush);
			}
			else {
				SelectObject(hDC, GetStockObject(NULL_BRUSH));
			}
			Polygon(hDC, SixPoints, 7);
			break;
		}

		case DRAWHEART: {
			POINT HeartPoints[6] = {
			   {newLine->x0,newLine->y0 + ((HIWORD(lParam) - newLine->y0) * 1 / 4)},
			   {newLine->x0 + ((LOWORD(lParam) - newLine->x0) * 1 / 5),newLine->y0},
			   {(LOWORD(lParam) + newLine->x0) / 2, newLine->y0 + ((HIWORD(lParam) - newLine->y0) * 1 / 4)},
			   {newLine->x0 + ((LOWORD(lParam) - newLine->x0) * 4 / 5),newLine->y0},
			   {LOWORD(lParam),newLine->y0 + ((HIWORD(lParam) - newLine->y0) * 1 / 4)},
			   {(LOWORD(lParam) + newLine->x0) / 2,HIWORD(lParam)}
			};

			if (newLine->isFill) {
				LogBrush.lbColor = g_drawcolor;
				LogBrush.lbHatch = HS_VERTICAL;
				LogBrush.lbStyle = BS_SOLID;
				HBRUSH brush = CreateBrushIndirect(&LogBrush);
				SelectObject(hDC, brush);
			}
			else {
				SelectObject(hDC, GetStockObject(NULL_BRUSH));
			}
			Polygon(hDC, HeartPoints, 6);
			break;
		}

		case DRAWDIAMOND: {
			POINT DiamondPoints[4] = {
			   {newLine->x0, newLine->y0 + ((HIWORD(lParam) - newLine->y0) * 1 / 2)},
			   {(LOWORD(lParam) + newLine->x0) / 2, newLine->y0},
			   {LOWORD(lParam), newLine->y0 + ((HIWORD(lParam) - newLine->y0) * 1 / 2)},
			   {(LOWORD(lParam) + newLine->x0) / 2, HIWORD(lParam)}
			};

			if (newLine->isFill) {
				LogBrush.lbColor = g_drawcolor;
				LogBrush.lbHatch = HS_VERTICAL;
				LogBrush.lbStyle = BS_SOLID;
				HBRUSH brush = CreateBrushIndirect(&LogBrush);
				SelectObject(hDC, brush);
			}
			else {
				SelectObject(hDC, GetStockObject(NULL_BRUSH));
			}
			Polygon(hDC, DiamondPoints, 4);
			break;
		}

		case DRAWRARROW: {
			POINT RarrowPoints[9] = {
			   {newLine->x0,   newLine->y0 + ((HIWORD(lParam) - newLine->y0) * 1 / 2)},
			   {newLine->x0,   newLine->y0 + ((HIWORD(lParam) - newLine->y0) * 1 / 4)},
			   {(LOWORD(lParam) + newLine->x0) / 2,   newLine->y0 + ((HIWORD(lParam) - newLine->y0) * 1 / 4)},
			   {(LOWORD(lParam) + newLine->x0) / 2,   newLine->y0},
			   {LOWORD(lParam),   newLine->y0 + ((HIWORD(lParam) - newLine->y0) * 1 / 2)},
			   {(LOWORD(lParam) + newLine->x0) / 2, HIWORD(lParam)},
			   {(LOWORD(lParam) + newLine->x0) / 2, newLine->y0 + ((HIWORD(lParam) - newLine->y0) * 3 / 4)},
			   {newLine->x0,   newLine->y0 + ((HIWORD(lParam) - newLine->y0) * 3 / 4)},
			   {   newLine->x0,   newLine->y0 + ((HIWORD(lParam) - newLine->y0) * 1 / 2)}
			};

			if (newLine->isFill) {
				LogBrush.lbColor = g_drawcolor;
				LogBrush.lbHatch = HS_VERTICAL;
				LogBrush.lbStyle = BS_SOLID;
				HBRUSH brush = CreateBrushIndirect(&LogBrush);
				SelectObject(hDC, brush);
			}
			else {
				SelectObject(hDC, GetStockObject(NULL_BRUSH));
			}
			Polygon(hDC, RarrowPoints, 9);
			break;
		}
		case DRAWCROWN: {
			POINT CrownPoints[9] = {
			   {newLine->x0, (HIWORD(lParam) - newLine->y0) / 3 + newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) / 8 + newLine->x0, HIWORD(lParam)},
			   {(LOWORD(lParam) - newLine->x0) * 7 / 8 + newLine->x0, HIWORD(lParam)},
			   {LOWORD(lParam), (HIWORD(lParam) - newLine->y0) / 3 + newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) * 3 / 4 + newLine->x0,(HIWORD(lParam) - newLine->y0) * 2 / 3 + newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) / 2 + newLine->x0, newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) * 1 / 4 + newLine->x0,(HIWORD(lParam) - newLine->y0) * 2 / 3 + newLine->y0},
			   {newLine->x0,(HIWORD(lParam) - newLine->y0) / 3 + newLine->y0 },
			   {newLine->x0, (HIWORD(lParam) - newLine->y0) / 3 + newLine->y0}
			};

			if (newLine->isFill) {
				LogBrush.lbColor = g_drawcolor;
				LogBrush.lbHatch = HS_VERTICAL;
				LogBrush.lbStyle = BS_SOLID;
				HBRUSH brush = CreateBrushIndirect(&LogBrush);
				SelectObject(hDC, brush);
			}
			else {
				SelectObject(hDC, GetStockObject(NULL_BRUSH));
			}
			Polygon(hDC, CrownPoints, 9);
			break;
		}

		case DRAWCROSS: {
			POINT CrossPoints[14] = {
			   {newLine->x0,(HIWORD(lParam) - newLine->y0) / 3 + newLine->y0},
			   {newLine->x0,(HIWORD(lParam) - newLine->y0) * 2 / 3 + newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) / 3 + newLine->x0,(HIWORD(lParam) - newLine->y0) * 2 / 3 + newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) / 3 + newLine->x0,HIWORD(lParam)},
			   {(LOWORD(lParam) - newLine->x0) * 2 / 3 + newLine->x0,HIWORD(lParam)},
			   {(LOWORD(lParam) - newLine->x0) * 2 / 3 + newLine->x0,(HIWORD(lParam) - newLine->y0) * 2 / 3 + newLine->y0},
			   {LOWORD(lParam),(HIWORD(lParam) - newLine->y0) * 2 / 3 + newLine->y0},
			   {LOWORD(lParam), (HIWORD(lParam) - newLine->y0) / 3 + newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) * 2 / 3 + newLine->x0,(HIWORD(lParam) - newLine->y0) / 3 + newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) * 2 / 3 + newLine->x0,newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) / 3 + newLine->x0,newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) / 3 + newLine->x0, (HIWORD(lParam) - newLine->y0) / 3 + newLine->y0},
			   {newLine->x0,(HIWORD(lParam) - newLine->y0) / 3 + newLine->y0 },
			   {newLine->x0,(HIWORD(lParam) - newLine->y0) / 3 + newLine->y0}
			};

			if (newLine->isFill) {
				LogBrush.lbColor = g_drawcolor;
				LogBrush.lbHatch = HS_VERTICAL;
				LogBrush.lbStyle = BS_SOLID;
				HBRUSH brush = CreateBrushIndirect(&LogBrush);
				SelectObject(hDC, brush);
			}
			else {
				SelectObject(hDC, GetStockObject(NULL_BRUSH));
			}
			Polygon(hDC, CrossPoints, 14);
			break;
		}

		case DRAWMULTI: {
			POINT MultiPoints[14] = {
			   {newLine->x0, (HIWORD(lParam) - newLine->y0) / 4 + newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) / 4 + newLine->x0, (HIWORD(lParam) - newLine->y0) / 2 + newLine->y0},
			   {newLine->x0, (HIWORD(lParam) - newLine->y0) * 3 / 4 + newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) / 4 + newLine->x0,HIWORD(lParam)},
			   {(LOWORD(lParam) - newLine->x0) / 2 + newLine->x0,(HIWORD(lParam) - newLine->y0) * 3 / 4 + newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) * 3 / 4 + newLine->x0,HIWORD(lParam)},
			   {LOWORD(lParam),(HIWORD(lParam) - newLine->y0) * 3 / 4 + newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) * 3 / 4 + newLine->x0,(HIWORD(lParam) - newLine->y0) / 2 + newLine->y0},
			   {LOWORD(lParam),(HIWORD(lParam) - newLine->y0) / 4 + newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) * 3 / 4 + newLine->x0,newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) / 2 + newLine->x0,(HIWORD(lParam) - newLine->y0) / 4 + newLine->y0 },
			   {(LOWORD(lParam) - newLine->x0) / 4 + newLine->x0,newLine->y0},
			   {newLine->x0,(HIWORD(lParam) - newLine->y0) / 4 + newLine->y0},
			   {newLine->x0, (HIWORD(lParam) - newLine->y0) / 4 + newLine->y0}
			};

			if (newLine->isFill) {
				LogBrush.lbColor = g_drawcolor;
				LogBrush.lbHatch = HS_VERTICAL;
				LogBrush.lbStyle = BS_SOLID;
				HBRUSH brush = CreateBrushIndirect(&LogBrush);
				SelectObject(hDC, brush);
			}
			else {
				SelectObject(hDC, GetStockObject(NULL_BRUSH));
			}
			Polygon(hDC, MultiPoints, 14);
			break;
		}
		default:
			break;
		}
		SelectObject(hDC, hOldPen);
		hPen = CreatePen(g_drawlinetype, newLine->lineWidth, g_drawcolor);

		// �޸� ��Ʈ�ʿ� �׸���
		hOldPen = (HPEN)SelectObject(hDCMem, hPen);

		switch (newLine->type) {
		case DRAWLINE:
			MoveToEx(hDCMem, newLine->x0, newLine->y0, NULL);
			LineTo(hDCMem, LOWORD(lParam), HIWORD(lParam));
			SelectObject(hDCMem, hOldPen);
			break;
		case DRAWTRI: {
			POINT trianglePoints[4] = {
			   { LOWORD(lParam), HIWORD(lParam) },
			   { newLine->x0, HIWORD(lParam) },
			   { (LOWORD(lParam) - newLine->x0) / 2 + newLine->x0, newLine->y0 },
			   { LOWORD(lParam), HIWORD(lParam) }
			};

			if (newLine->isFill) {
				LogBrush.lbColor = g_drawcolor;
				LogBrush.lbHatch = HS_VERTICAL;
				LogBrush.lbStyle = BS_SOLID;
				HBRUSH brush = CreateBrushIndirect(&LogBrush);
				SelectObject(hDCMem, brush);
			}
			else {
				SelectObject(hDCMem, hOldPen);
			}

			Polygon(hDCMem, trianglePoints, 4);
			SelectObject(hDCMem, hOldPen);
			break;
		}
		case DRAWRECT: {
			if (newLine->isFill) {
				LogBrush.lbColor = g_drawcolor;
				LogBrush.lbHatch = HS_VERTICAL;
				LogBrush.lbStyle = BS_SOLID;
				HBRUSH brush = CreateBrushIndirect(&LogBrush);
				SelectObject(hDCMem, brush);
			}
			else {
				SelectObject(hDCMem, hOldPen);
			}

			Rectangle(hDCMem, newLine->x0, newLine->y0, LOWORD(lParam), HIWORD(lParam));
			SelectObject(hDCMem, hOldPen);
			break;
		}
		case DRAWELLIPSE: {
			if (newLine->isFill) {
				LogBrush.lbColor = g_drawcolor;
				LogBrush.lbHatch = HS_VERTICAL;
				LogBrush.lbStyle = BS_SOLID;
				HBRUSH brush = CreateBrushIndirect(&LogBrush);
				SelectObject(hDCMem, brush);
			}
			else {
				SelectObject(hDCMem, hOldPen);
			}

			Ellipse(hDCMem, newLine->x0, newLine->y0, LOWORD(lParam), HIWORD(lParam));
			SelectObject(hDCMem, hOldPen);
			break;
		}
		case DRAWSTRICTLINE: {
			MoveToEx(hDCMem, newLine->x0, newLine->y0, NULL);
			LineTo(hDCMem, newLine->x1, newLine->y1);
			break;
		}
		case DRAWPENTAGON: {
			POINT PolyPoints[6] = {
			   { newLine->x0, (newLine->y0 > HIWORD(lParam) ? (newLine->y0 - HIWORD(lParam)) / 3 + HIWORD(lParam) : (HIWORD(lParam) - newLine->y0) / 3 + newLine->y0) },
			{ ((LOWORD(lParam) - newLine->x0) * 2 / 9 + newLine->x0), (newLine->y0 > HIWORD(lParam) ? newLine->y0 : HIWORD(lParam)) },
			{ ((LOWORD(lParam) - newLine->x0) * 7 / 9 + newLine->x0), (newLine->y0 > HIWORD(lParam) ? newLine->y0 : HIWORD(lParam)) },
			{ LOWORD(lParam), (newLine->y0 > HIWORD(lParam) ? (newLine->y0 - HIWORD(lParam)) / 3 + HIWORD(lParam) : (HIWORD(lParam) - newLine->y0) / 3 + newLine->y0) },
			{ ((LOWORD(lParam) + newLine->x0) / 2), (newLine->y0 > HIWORD(lParam) ? HIWORD(lParam) : newLine->y0) },
			{ newLine->x0, (newLine->y0 > HIWORD(lParam) ? (newLine->y0 - HIWORD(lParam)) / 3 + HIWORD(lParam) : (HIWORD(lParam) - newLine->y0) / 3 + newLine->y0) }
			};

			if (newLine->isFill) {
				LogBrush.lbColor = g_drawcolor;
				LogBrush.lbHatch = HS_VERTICAL;
				LogBrush.lbStyle = BS_SOLID;
				HBRUSH brush = CreateBrushIndirect(&LogBrush);
				SelectObject(hDCMem, brush);
			}
			else {
				SelectObject(hDCMem, GetStockObject(NULL_BRUSH));
			}
			Polygon(hDCMem, PolyPoints, 6);
			SelectObject(hDCMem, hOldPen);
			break;
		}
		case DRAWSTAR: {
			POINT STARPoints[] = {
			   {(newLine->x0 + (LOWORD(lParam) - newLine->x0) / 2), newLine->y0 },
			   { (newLine->x0 + (LOWORD(lParam) - newLine->x0) * 3 / 8), (newLine->y0 + (HIWORD(lParam) - newLine->y0) * 3 / 8) },
			   { newLine->x0, (newLine->y0 + (HIWORD(lParam) - newLine->y0) * 3 / 8) },
			   { (newLine->x0 + (LOWORD(lParam) - newLine->x0) * 5 / 16), (HIWORD(lParam) - (HIWORD(lParam) - newLine->y0) * 3 / 8) },
			   { (newLine->x0 + (LOWORD(lParam) - newLine->x0) * 1 / 8), HIWORD(lParam) },
			   { (newLine->x0 + (LOWORD(lParam) - newLine->x0) / 2), (HIWORD(lParam) - (HIWORD(lParam) - newLine->y0) * 1 / 4) },
			   { (LOWORD(lParam) - (LOWORD(lParam) - newLine->x0) * 1 / 8), HIWORD(lParam) },
			   { (LOWORD(lParam) - (LOWORD(lParam) - newLine->x0) * 5 / 16), (HIWORD(lParam) - (HIWORD(lParam) - newLine->y0) * 3 / 8) },
			   { LOWORD(lParam), (newLine->y0 + (HIWORD(lParam) - newLine->y0) * 3 / 8) },
			   { (LOWORD(lParam) - (LOWORD(lParam) - newLine->x0) * 3 / 8), (newLine->y0 + (HIWORD(lParam) - newLine->y0) * 3 / 8) }
			};

			if (newLine->isFill) {
				LogBrush.lbColor = g_drawcolor;
				LogBrush.lbHatch = HS_VERTICAL;
				LogBrush.lbStyle = BS_SOLID;
				HBRUSH brush = CreateBrushIndirect(&LogBrush);
				SelectObject(hDCMem, brush);
			}
			else {
				SelectObject(hDCMem, GetStockObject(NULL_BRUSH));
			}
			Polygon(hDCMem, STARPoints, 10);
			SelectObject(hDCMem, hOldPen);
			break;
		}
		case DRAWHEXAGON: {
			POINT AnyPoints[7] = {
			   {newLine->x0, (HIWORD(lParam) - newLine->y0) / 4 + newLine->y0},
			   {newLine->x0, (HIWORD(lParam) - newLine->y0) * 3 / 4 + newLine->y0, },
			   {(LOWORD(lParam) - newLine->x0) / 2 + newLine->x0, HIWORD(lParam)},
			   {LOWORD(lParam), (HIWORD(lParam) - newLine->y0) * 3 / 4 + newLine->y0  },
			   {LOWORD(lParam), (HIWORD(lParam) - newLine->y0) / 4 + newLine->y0  },
			   {(LOWORD(lParam) - newLine->x0) / 2 + newLine->x0,  newLine->y0   },
			   { newLine->x0, (HIWORD(lParam) - newLine->y0) / 4 + newLine->y0,}
			};

			if (newLine->isFill) {
				LogBrush.lbColor = g_drawcolor;
				LogBrush.lbHatch = HS_VERTICAL;
				LogBrush.lbStyle = BS_SOLID;
				HBRUSH brush = CreateBrushIndirect(&LogBrush);
				SelectObject(hDCMem, brush);
			}
			else {
				SelectObject(hDCMem, hOldPen);
			}
			Polygon(hDCMem, AnyPoints, 7);
			SelectObject(hDCMem, hOldPen);
			break;
		}
		case DRAWHEART: {
			POINT HeartPoints[6] = {
			   {newLine->x0,newLine->y0 + ((HIWORD(lParam) - newLine->y0) * 1 / 4)},
			   {newLine->x0 + ((LOWORD(lParam) - newLine->x0) * 1 / 5),newLine->y0},
			   {(LOWORD(lParam) + newLine->x0) / 2, newLine->y0 + ((HIWORD(lParam) - newLine->y0) * 1 / 4)},
			   {newLine->x0 + ((LOWORD(lParam) - newLine->x0) * 4 / 5),newLine->y0},
			   {LOWORD(lParam),newLine->y0 + ((HIWORD(lParam) - newLine->y0) * 1 / 4)},
			   {(LOWORD(lParam) + newLine->x0) / 2,HIWORD(lParam)}
			};

			if (newLine->isFill) {
				LogBrush.lbColor = g_drawcolor;
				LogBrush.lbHatch = HS_VERTICAL;
				LogBrush.lbStyle = BS_SOLID;
				HBRUSH brush = CreateBrushIndirect(&LogBrush);
				SelectObject(hDCMem, brush);
			}
			else {
				SelectObject(hDCMem, hOldPen);
			}
			Polygon(hDCMem, HeartPoints, 6);
			SelectObject(hDCMem, hOldPen);
			break;
		}
		case DRAWDIAMOND: {
			POINT DiamondPoints[4] = {
			   {newLine->x0, newLine->y0 + ((HIWORD(lParam) - newLine->y0) * 1 / 2)},
			   {(LOWORD(lParam) + newLine->x0) / 2, newLine->y0},
			   {LOWORD(lParam), newLine->y0 + ((HIWORD(lParam) - newLine->y0) * 1 / 2)},
			   {(LOWORD(lParam) + newLine->x0) / 2, HIWORD(lParam)}
			};

			if (newLine->isFill) {
				LogBrush.lbColor = g_drawcolor;
				LogBrush.lbHatch = HS_VERTICAL;
				LogBrush.lbStyle = BS_SOLID;
				HBRUSH brush = CreateBrushIndirect(&LogBrush);
				SelectObject(hDCMem, brush);
			}
			else {
				SelectObject(hDCMem, GetStockObject(NULL_BRUSH));
			}
			Polygon(hDCMem, DiamondPoints, 4);
			SelectObject(hDCMem, hOldPen);
			break;
		}

		case DRAWRARROW: {
			POINT RarrowPoints[9] = {
			   {newLine->x0,   newLine->y0 + ((HIWORD(lParam) - newLine->y0) * 1 / 2)},
			   {newLine->x0,   newLine->y0 + ((HIWORD(lParam) - newLine->y0) * 1 / 4)},
			   {(LOWORD(lParam) + newLine->x0) / 2,   newLine->y0 + ((HIWORD(lParam) - newLine->y0) * 1 / 4)},
			   {(LOWORD(lParam) + newLine->x0) / 2,   newLine->y0},
			   {LOWORD(lParam),   newLine->y0 + ((HIWORD(lParam) - newLine->y0) * 1 / 2)},
			   {(LOWORD(lParam) + newLine->x0) / 2, HIWORD(lParam)},
			   {(LOWORD(lParam) + newLine->x0) / 2, newLine->y0 + ((HIWORD(lParam) - newLine->y0) * 3 / 4)},
			   {newLine->x0,   newLine->y0 + ((HIWORD(lParam) - newLine->y0) * 3 / 4)},
			   {newLine->x0,   newLine->y0 + ((HIWORD(lParam) - newLine->y0) * 1 / 2)   }
			};

			if (newLine->isFill) {
				LogBrush.lbColor = g_drawcolor;
				LogBrush.lbHatch = HS_VERTICAL;
				LogBrush.lbStyle = BS_SOLID;
				HBRUSH brush = CreateBrushIndirect(&LogBrush);
				SelectObject(hDCMem, brush);
			}
			else {
				SelectObject(hDCMem, GetStockObject(NULL_BRUSH));
			}
			Polygon(hDCMem, RarrowPoints, 9);
			SelectObject(hDCMem, hOldPen);
			break;
		}
		case DRAWCROWN: {
			POINT CrownPoints[9] = {
			   {newLine->x0, (HIWORD(lParam) - newLine->y0) / 3 + newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) / 8 + newLine->x0, HIWORD(lParam)},
			   {(LOWORD(lParam) - newLine->x0) * 7 / 8 + newLine->x0, HIWORD(lParam)},
			   {LOWORD(lParam), (HIWORD(lParam) - newLine->y0) / 3 + newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) * 3 / 4 + newLine->x0,(HIWORD(lParam) - newLine->y0) * 2 / 3 + newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) / 2 + newLine->x0, newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) * 1 / 4 + newLine->x0,(HIWORD(lParam) - newLine->y0) * 2 / 3 + newLine->y0},
			   {newLine->x0,(HIWORD(lParam) - newLine->y0) / 3 + newLine->y0 },
			   {newLine->x0, (HIWORD(lParam) - newLine->y0) / 3 + newLine->y0}
			};

			if (newLine->isFill) {
				LogBrush.lbColor = g_drawcolor;
				LogBrush.lbHatch = HS_VERTICAL;
				LogBrush.lbStyle = BS_SOLID;
				HBRUSH brush = CreateBrushIndirect(&LogBrush);
				SelectObject(hDCMem, brush);
			}
			else {
				SelectObject(hDCMem, GetStockObject(NULL_BRUSH));
			}
			Polygon(hDCMem, CrownPoints, 9);
			SelectObject(hDCMem, hOldPen);
			break;
		}

		case DRAWCROSS: {
			POINT CrossPoints[14] = {
			   {newLine->x0,(HIWORD(lParam) - newLine->y0) / 3 + newLine->y0},
			   {newLine->x0,(HIWORD(lParam) - newLine->y0) * 2 / 3 + newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) / 3 + newLine->x0,(HIWORD(lParam) - newLine->y0) * 2 / 3 + newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) / 3 + newLine->x0,HIWORD(lParam)},
			   {(LOWORD(lParam) - newLine->x0) * 2 / 3 + newLine->x0,HIWORD(lParam)},
			   {(LOWORD(lParam) - newLine->x0) * 2 / 3 + newLine->x0,(HIWORD(lParam) - newLine->y0) * 2 / 3 + newLine->y0},
			   {LOWORD(lParam),(HIWORD(lParam) - newLine->y0) * 2 / 3 + newLine->y0},
			   {LOWORD(lParam), (HIWORD(lParam) - newLine->y0) / 3 + newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) * 2 / 3 + newLine->x0,(HIWORD(lParam) - newLine->y0) / 3 + newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) * 2 / 3 + newLine->x0,newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) / 3 + newLine->x0,newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) / 3 + newLine->x0, (HIWORD(lParam) - newLine->y0) / 3 + newLine->y0},
			   {newLine->x0,(HIWORD(lParam) - newLine->y0) / 3 + newLine->y0 },
			   {newLine->x0,(HIWORD(lParam) - newLine->y0) / 3 + newLine->y0}
			};

			if (newLine->isFill) {
				LogBrush.lbColor = g_drawcolor;
				LogBrush.lbHatch = HS_VERTICAL;
				LogBrush.lbStyle = BS_SOLID;
				HBRUSH brush = CreateBrushIndirect(&LogBrush);
				SelectObject(hDCMem, brush);
			}
			else {
				SelectObject(hDCMem, GetStockObject(NULL_BRUSH));
			}
			Polygon(hDCMem, CrossPoints, 14);
			SelectObject(hDCMem, hOldPen);
			break;
		}

		case DRAWMULTI: {
			POINT MultiPoints[14] = {
			   {newLine->x0, (HIWORD(lParam) - newLine->y0) / 4 + newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) / 4 + newLine->x0, (HIWORD(lParam) - newLine->y0) / 2 + newLine->y0},
			   {newLine->x0, (HIWORD(lParam) - newLine->y0) * 3 / 4 + newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) / 4 + newLine->x0,HIWORD(lParam)},
			   {(LOWORD(lParam) - newLine->x0) / 2 + newLine->x0,(HIWORD(lParam) - newLine->y0) * 3 / 4 + newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) * 3 / 4 + newLine->x0,HIWORD(lParam)},
			   {LOWORD(lParam),(HIWORD(lParam) - newLine->y0) * 3 / 4 + newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) * 3 / 4 + newLine->x0,(HIWORD(lParam) - newLine->y0) / 2 + newLine->y0},
			   {LOWORD(lParam),(HIWORD(lParam) - newLine->y0) / 4 + newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) * 3 / 4 + newLine->x0,newLine->y0},
			   {(LOWORD(lParam) - newLine->x0) / 2 + newLine->x0,(HIWORD(lParam) - newLine->y0) / 4 + newLine->y0 },
			   {(LOWORD(lParam) - newLine->x0) / 4 + newLine->x0,newLine->y0},
			   {newLine->x0,(HIWORD(lParam) - newLine->y0) / 4 + newLine->y0},
			   {newLine->x0, (HIWORD(lParam) - newLine->y0) / 4 + newLine->y0}
			};

			if (newLine->isFill) {
				LogBrush.lbColor = g_drawcolor;
				LogBrush.lbHatch = HS_VERTICAL;
				LogBrush.lbStyle = BS_SOLID;
				HBRUSH brush = CreateBrushIndirect(&LogBrush);
				SelectObject(hDCMem, brush);
			}
			else {
				SelectObject(hDCMem, GetStockObject(NULL_BRUSH));
			}
			Polygon(hDCMem, MultiPoints, 14);
			SelectObject(hDCMem, hOldPen);
			break;
		}
		default:
			break;
		}
		SelectObject(hDC, hOldPen);

		DeleteObject(hPen);
		ReleaseDC(hWnd, hDC);
		return 0;
	}

	case WM_PAINT:
		hDC = BeginPaint(hWnd, &ps);

		// �޸� ��Ʈ�ʿ� ����� �׸��� ȭ�鿡 ����
		GetClientRect(hWnd, &rect);
		BitBlt(hDC, 0, 0, rect.right - rect.left, rect.bottom - rect.top, hDCMem, 0, 0, SRCCOPY);

		EndPaint(hWnd, &ps);
		return 0;
	case WM_DESTROY:
		DeleteObject(hBitmap);
		DeleteDC(hDCMem);
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

// ����Ʈ ��Ʈ�ѿ� ���ڿ� ���
void DisplayText(char* fmt, ...) {
	va_list arg;
	va_start(arg, fmt);

	char cbuf[1024];
	vsprintf(cbuf, fmt, arg);

	int nLength = GetWindowTextLength(g_hEditStatus);
	SendMessage(g_hEditStatus, EM_SETSEL, nLength, nLength);
	SendMessage(g_hEditStatus, EM_REPLACESEL, FALSE, (LPARAM)cbuf);

	va_end(arg);
}

// ����� ���� ������ ���� �Լ�
int recvn(SOCKET s, char* buf, int len, int flags) {
	int received;
	char* ptr = buf;
	int left = len;

	while (left > 0) {
		received = recv(s, ptr, left, flags);
		if (received == SOCKET_ERROR)
			return SOCKET_ERROR;
		else if (received == 0)
			break;
		left -= received;
		ptr += received;
	}

	return (len - left);
}


// ���� ǥ�ø� ���� �Լ�.
int CalculateLog2(short num) {
	if (num == 1) {
		return 0;
	}
	else if (num == 2 || num == 4 || num == 8) {
		return log2(num);
	}
	else {
		return 4;
	}
}
//Multicast group receiving thread
DWORD WINAPI ReceiveProcess(LPVOID arg) {
	int retval;

	// socket()
	SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET) err_quit("socket()");
	g_multicast_sock = sock;

	// SO_REUSEADDR �ɼ� ����
	BOOL optval = TRUE;
	retval = setsockopt(sock, SOL_SOCKET,
		SO_REUSEADDR, (char*)&optval, sizeof(optval));
	if (retval == SOCKET_ERROR) err_quit("setsockopt()");

	// bind()
	SOCKADDR_IN localaddr;
	ZeroMemory(&localaddr, sizeof(localaddr));
	localaddr.sin_family = AF_INET;
	localaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	localaddr.sin_port = htons(9000);
	retval = bind(sock, (SOCKADDR*)&localaddr, sizeof(localaddr));
	if (retval == SOCKET_ERROR) err_quit("bind()");


	COMM_MSG comm_msg;
	CHAT_MSG chat_msg;
	DRAWLINE_MSG draw_msg;

	while (1) {
		retval = recvn(sock, (char*)&comm_msg, sizeof(COMM_MSG), 0);
		if (retval == 0 || retval == SOCKET_ERROR) {
			break;
		}

		if (comm_msg.type == CHATTING) {
			retval = recvn(sock, (char*)&chat_msg, comm_msg.size, 0);
			if (retval == 0 || retval == SOCKET_ERROR) {
				break;
			}

			DisplayText("[%s/%s] %s\r\n", chat_msg.nickname, groupIP[CalculateLog2(comm_msg.ip_addr_str)], chat_msg.buf);

		}
		else if (comm_msg.type != CHATTING) {
			retval = recvn(sock, (char*)&draw_msg, comm_msg.size, 0);
			if (retval == 0 || retval == SOCKET_ERROR) {
				break;
			}
			g_drawcolor = draw_msg.color;
			g_drawlinetype = draw_msg.linetype;

			SendMessage(g_hDrawWnd, WM_DRAWIT, (WPARAM)&draw_msg, MAKELPARAM(draw_msg.x1, draw_msg.y1));
		}
	}
	return 0;
}

// ���� �Լ� ���� ��� �� ����
void err_quit(char* msg) {
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, (LPCSTR)msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(1);
}

// ���� �Լ� ���� ���
void err_display(char* msg) {
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[%s] %s", msg, (char*)lpMsgBuf);
	LocalFree(lpMsgBuf);
}


void enableConnectivity() {
	Button_SetText(hButtonConnect, "������ �����ϱ�");
	EnableWindow(hEditIPaddr, TRUE);
	EnableWindow(hEditPort, TRUE);
	EnableWindow(g_hButtonSendMsg, TRUE);
	EnableWindow(g_hButtonRemovemsg, TRUE);
	EnableWindow(hEditUserID, TRUE);
	EnableWindow(hGroupCombo, FALSE);
	EnableWindow(hAddToGroup, FALSE);
	EnableWindow(hRemFromGroup, FALSE);
}

void disableConnectivity() {
	EnableWindow(hEditIPaddr, FALSE);
	EnableWindow(hEditPort, FALSE);
	EnableWindow(g_hButtonSendMsg, FALSE);
	EnableWindow(g_hButtonRemovemsg, FALSE);
	EnableWindow(hEditUserID, FALSE);
	EnableWindow(hGroupCombo, TRUE);
	EnableWindow(hAddToGroup, TRUE);
	EnableWindow(hRemFromGroup, TRUE);
}
void enableSendButtonAfterJoiningGroup() {
	EnableWindow(g_hButtonSendMsg, TRUE);
	EnableWindow(g_hButtonRemovemsg, TRUE);
}
void disableSendButtonAfterJoiningGroup() {
	EnableWindow(g_hButtonSendMsg, FALSE);
	EnableWindow(g_hButtonRemovemsg, FALSE);
}
