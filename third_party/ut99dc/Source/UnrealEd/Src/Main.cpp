/*=============================================================================
	Main.cpp: UnrealEd Windows startup.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
=============================================================================*/

#pragma warning( disable : 4201 )
#define STRICT
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include "Engine.h"
#include "UnRender.h"
#include "Window.h"
#include "..\..\Editor\Src\EditorPrivate.h"
#include "Res\resource.h"
#include "UnEngineWin.h"

extern "C" {HINSTANCE hInstance;}
extern "C" {TCHAR GPackage[64]=TEXT("UnrealEd");}

// Brushes.
HBRUSH hBrushMode = CreateSolidBrush( RGB(0,96,0) );

// Docking frame positions.
enum EDockingFramePosition
{
	DOCKF_Top=0,
	DOCKF_Bottom=1,
	DOCKF_Left=2,
	DOCKF_Right=3,
};

// Classes.
class WMdiClient;
class WMdiFrame;
class WEditorFrame;
class WMdiDockingFrame;

// Memory allocator.
#include "FMallocWindows.h"
FMallocWindows Malloc;

// Log file.
#include "FOutputDeviceFile.h"
FOutputDeviceFile Log;

// Error handler.
#include "FOutputDeviceWindowsError.h"
FOutputDeviceWindowsError Error;

// Feedback.
#include "FFeedbackContextWindows.h"
FFeedbackContextWindows Warn;

// File manager.
#include "FFileManagerWindows.h"
FFileManagerWindows FileManager;

// Config.
#include "FConfigCacheIni.h"

/*-----------------------------------------------------------------------------
	Document manager crappy abstraction.
-----------------------------------------------------------------------------*/

struct FDocumentManager
{
	virtual void OpenLevelView()=0;
	virtual void OpenScriptView( UClass* Class )=0;
} *GDocumentManager=NULL;

/*-----------------------------------------------------------------------------
	WMdiClient.
-----------------------------------------------------------------------------*/

// An MDI client window.
class WMdiClient : public WControl
{
	DECLARE_WINDOWSUBCLASS(WMdiClient,WControl,UnrealEd)
	WMdiClient( WWindow* InOwner )
	: WControl( InOwner, 0, SuperProc )
	{}
	void OpenWindow( CLIENTCREATESTRUCT* ccs )
	{
		guard(WMdiFrame::OpenWindow);
		//must make nccreate work!! GetWindowClassName(),
		//!! WS_VSCROLL | WS_HSCROLL
        HWND hWndCreated = TCHAR_CALL_OS(CreateWindowEx(0,TEXT("MDICLIENT"),NULL,WS_CHILD|WS_CLIPCHILDREN | WS_CLIPSIBLINGS,0,0,0,0,OwnerWindow->hWnd,(HMENU)0xCAC,hInstance,ccs),CreateWindowExA(0,"MDICLIENT",NULL,WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,0,0,0,0,OwnerWindow->hWnd,(HMENU)0xCAC,hInstance,ccs));
		check(hWndCreated);
		check(!hWnd);
		_Windows.AddItem( this );
		hWnd = hWndCreated;
		Show( 1 );
		unguard;
	}
};
WNDPROC WMdiClient::SuperProc;

/*-----------------------------------------------------------------------------
	WDockingFrame.
-----------------------------------------------------------------------------*/

// One of four docking frame windows on a MDI frame.
class WDockingFrame : public WWindow
{
	DECLARE_WINDOWCLASS(WDockingFrame,WWindow,UnrealEd)

	// Variables.
	EDockingFramePosition DockingPosition;
	INT DockDepth;
	WWindow* Child;

	// Functions.
	WDockingFrame( FName InPersistentName, WMdiFrame* InFrame, INT InDockDepth, EDockingFramePosition InPos )
	:	WWindow			( InPersistentName, (WWindow*)InFrame )
	,	DockingPosition	( InPos )
	,   DockDepth       ( InDockDepth )
	,	Child			( NULL )
	{}
	void OpenWindow()
	{
		guard(WDockingFrame::OpenWindow);
		PerformCreateWindowEx
		(
			0,
			NULL,
			WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
			0, 0, 0, 0,
			OwnerWindow->hWnd,
			NULL,
			hInstance
		);
		Show(1);
		unguard;
	}
	void Dock( WWindow* InChild )
	{
		guard(WDockingFrame::Dock);
		Child = InChild;
		unguard;
	}
	void OnSize( DWORD Flags, INT InX, INT InY )
	{
		guard(WDockingFrame::OnSize);
		if( Child )
			Child->MoveWindow( GetClientRect(), TRUE );
		unguard;
	}
	void OnPaint()
	{
		guard(WDockingFrame::OnPaint);
		PAINTSTRUCT PS;
		HDC hDC = BeginPaint( *this, &PS );

		FRect Rect = GetClientRect();
		FillRect( hDC, Rect, (HBRUSH)(COLOR_BTNFACE+1) );
		DrawEdge( hDC, Rect, BDR_RAISEDINNER, BF_TOPLEFT|BF_BOTTOMRIGHT );

		EndPaint( *this, &PS );
		unguard;
	}
};

/*-----------------------------------------------------------------------------
	WMdiFrame.
-----------------------------------------------------------------------------*/

// An MDI frame window.
class WMdiFrame : public WWindow
{
	DECLARE_WINDOWCLASS(WMdiFrame,WWindow,UnrealEd)

	// Variables.
	WMdiClient MdiClient;
	WDockingFrame TopFrame, BottomFrame, LeftFrame, RightFrame;

	// Functions.
	WMdiFrame( FName InPersistentName )
	:	WWindow		( InPersistentName )
	,	MdiClient	( this )
	,	TopFrame	( TEXT("MdiFrameTop"),	  this, 32,  DOCKF_Top )
	,	BottomFrame	( TEXT("MdiFrameBottom"), this, 0,   DOCKF_Bottom )
	,	LeftFrame	( TEXT("MdiFrameLeft"),	  this, 224, DOCKF_Left )
	,	RightFrame	( TEXT("MdiFrameRight"),  this, 224, DOCKF_Right )
	{}
	INT CallDefaultProc( UINT Message, UINT wParam, LONG lParam )
	{
		return DefFrameProcX( hWnd, MdiClient.hWnd, Message, wParam, lParam );
	}
	void OnCreate()
	{
		guard(WMdiFrame::OnCreate);
		WWindow::OnCreate();

		// Create docking frames.
		TopFrame   .OpenWindow();
		BottomFrame.OpenWindow();
		LeftFrame  .OpenWindow();
		RightFrame .OpenWindow();

		unguard;
	}
	virtual void RepositionClient()
	{
		guard(WMdiFrame::RepositionClient);

		// Reposition docking frames.
		FRect Client = GetClientRect();
		TopFrame   .MoveWindow( FRect(0, 0, Client.Max.X, TopFrame.DockDepth), 1 );
		BottomFrame.MoveWindow( FRect(0, Client.Max.Y-BottomFrame.DockDepth, Client.Max.X, Client.Max.Y), 1 );
		LeftFrame  .MoveWindow( FRect(0, TopFrame.DockDepth, LeftFrame.DockDepth, Client.Max.Y-BottomFrame.DockDepth), 1 );
		RightFrame .MoveWindow( FRect(Client.Max.X-RightFrame.DockDepth, TopFrame.DockDepth, Client.Max.X, Client.Max.Y-BottomFrame.DockDepth), 1 );

		// Reposition MDI client window.
		MdiClient  .MoveWindow( FRect(LeftFrame.DockDepth, TopFrame.DockDepth, Client.Max.X-RightFrame.DockDepth, Client.Max.Y-BottomFrame.DockDepth), 1 );

		unguard;
	}
	void OnSize( DWORD Flags, INT NewX, INT NewY )
	{
		guard(WMdiFrame::OnSize);
		RepositionClient();
		throw TEXT("NoRoute");
		unguard;
	}
	void OpenWindow()
	{
		guard(WMdiFrame::OpenWindow);
		TCHAR Title[256];
		appSprintf( Title, LocalizeGeneral(TEXT("FrameWindow"),TEXT("UnrealEd")), LocalizeGeneral(TEXT("Product"),TEXT("Core")) );
		PerformCreateWindowEx
		(
			WS_EX_APPWINDOW,
			Title,
			WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX | WS_MAXIMIZEBOX | WS_MINIMIZEBOX,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			640,
			480,
			NULL,
			NULL,
			hInstance
		);
		ShowWindow( *this, SW_SHOWMAXIMIZED );
		unguard;
	}
	void OnSetFocus()
	{
		guard(WMdiFrame::OnSetFocus);
		SetFocus( MdiClient );
		unguard;
	}
};

/*-----------------------------------------------------------------------------
	WBackgroundHolder.
-----------------------------------------------------------------------------*/

// Test.
class WBackgroundHolder : public WWindow
{
	DECLARE_WINDOWCLASS(WBackgroundHolder,WWindow,Window)

	// Structors.
	WBackgroundHolder( FName InPersistentName, WWindow* InOwnerWindow )
	:	WWindow( InPersistentName, InOwnerWindow )
	{}

	// WWindow interface.
	void OpenWindow()
	{
		guard(WBackgroundHolder::OpenWindow);
		MdiChild = 0;
		PerformCreateWindowEx
		(
			WS_EX_TOOLWINDOW | WS_EX_WINDOWEDGE,
			NULL,
			WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
			0,
			0,
			512,
			256,
			OwnerWindow ? OwnerWindow->hWnd : NULL,
			NULL,
			hInstance
		);
		SetWindowLongX( hWnd, GWL_STYLE, WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS );
		unguard;
	}
};

/*-----------------------------------------------------------------------------
	WSplitter.
-----------------------------------------------------------------------------*/

// Cell splitter info.
struct FCellSplit
{
	INT Start;
	INT Size;
	FLOAT Fraction;
};

// A vertical splitter window.
class WSplitter : public WWindow
{
	DECLARE_WINDOWCLASS(WSplitter,WWindow,Window)

	// Variables.
	WMdiClient* MdiClient;
	FPoint Handle;
	FPoint Border;
	TArray<FCellSplit> SplitX, SplitY;
	TArray<WWindow*> Cells;
	WDragInterceptor* DragInterceptor;
	TArray<FCellSplit>& Split( INT i )
	{
		return (&SplitX)[i];
	}

	// Structors.
	WSplitter( WWindow* InOwnerWindow, INT InCellsX, INT InCellsY )
	:	WWindow( NAME_None, InOwnerWindow )
	,	SplitX( InCellsX )
	,	SplitY( InCellsY )
	,	Handle( 3, 3 )
	,	Border( 2, 2 )
	,	DragInterceptor( NULL )
	{
		guard(WSplitter::WSplitter);
		for( INT i=0; i<FPoint::Num(); i++ )
			for( INT j=0; j<Split(i).Num(); j++ )
				Split(i)(j).Fraction = 1.0/Split(i).Num();
		Cells.AddZeroed( SplitX.Num() * SplitY.Num() );
		unguard;
	}

	// WSplitter interface.
	FPoint GetDragIndices()
	{
		guard(WSplitter::GetDragIndices);
		FPoint Mouse  = GetCursorPos();
		FPoint Result = FPoint::NoneValue();
		for( INT i=0; i<FPoint::Num(); i++ )
			for( INT j=0; j<Split(i).Num()-1; j++ )
				if( Mouse(i)>=Split(i)(j+1).Start-Border(i)-Handle(i) && Mouse(i)<Split(i)(j+1).Start+Border(i) )
					Result(i) = j;
		return Result;
		unguard;
	}
	FRect GetSplitWindowRect( INT i, INT j )
	{
		guard(WSplitter::GetSplitWindowRect);
		FPoint Start( SplitX(i).Start, SplitY(j).Start );
		return FRect( Start, Start + FPoint(SplitX(i).Size,SplitY(j).Size) + Handle + Border + Border );
		unguard;
	}
	FRect GetSplitClientRect( INT i, INT j )
	{
		guard(WSplitter::GetSplitWindowRect);
		FRect Cell = GetSplitWindowRect( i, j );
		return FRect( Cell.Min+Border, Cell.Max-Border-Handle );
		unguard;
	}
	virtual void DrawCellFrames( HDC hInDC )
	{
		guard(WSplitter::DrawCellFrames);
		HDC hDC = hInDC ? hInDC : GetDC(hWnd);
		if( DragInterceptor )
			DragInterceptor->ToggleDraw( NULL );
		for( INT i=0; i<SplitY.Num(); i++ )
		{
			for( INT j=0; j<SplitX.Num(); j++ )
			{
				FRect Cell = GetSplitWindowRect(j,i);
				if( j<SplitX.Num()-1 )
					FillRect( hDC, Cell.Right(Handle.X), (HBRUSH)(COLOR_BTNFACE+1) );
				if( i<SplitY.Num()-1 )
					FillRect( hDC, Cell.Bottom(Handle.Y), (HBRUSH)(COLOR_BTNFACE+1) );
				DrawEdge( hDC, FRect(Cell.Min,Cell.Max-Handle), EDGE_SUNKEN, BF_RECT );
			}
		}
		if( DragInterceptor )
			DragInterceptor->ToggleDraw( NULL );
		if( !hInDC )
			ReleaseDC( hWnd, hDC );
		unguard;
	}

	// WWindow interface.
	void OnChar( TCHAR Ch )
	{
		if( Ch==0 )//!!temporary messaging hook from UViewports
		{
			DrawCellFrames( NULL );
			for( INT i=0; i<SplitY.Num(); i++ )
			{
				for( INT j=0; j<SplitX.Num(); j++ )
				{
					/*UViewport* Viewport = Cast<UViewport>(Cells(j+i*SplitX.Num()));
					if( Viewport && Viewport->Current )
					{
						SendMessageX( MdiClient->hWnd, WM_MDIACTIVATE, (WPARAM)OwnerWindow->hWnd, 0 );//!!hacked messaging system
						SendMessageX( OwnerWindow->hWnd, WM_NCACTIVATE, 1, 0 );
					}*/
				}
			}
		}
	}
	virtual void Serialize( FArchive& Ar )
	{
		guard(WSplitter::Serialize);
		WWindow::Serialize( Ar );
		for( INT i=0; i<Cells.Num(); i++ )
			if( Cells(i) )
				Cells(i)->Serialize( Ar );
		unguard;
	}
	void OpenWindow()
	{
		guard(WSplitter::OpenWindow);
		check(OwnerWindow);
		PerformCreateWindowEx
		(
			0,
			NULL,
			WS_CHILD | WS_VISIBLE,
			0, 0,
			0, 0,
			OwnerWindow->hWnd,
			NULL,
			hInstance
		);
		Resize( 1 );
		unguard;
	}
 	void Resize( UBOOL Recompute )
	{
		guard(WSplitter::Resize);
		FPoint New = OwnerWindow->GetClientRect().Size();
		for( INT i=0; i<FPoint::Num(); i++ )
		{
			INT Overhead = Split(i).Num() * (2*Border(i) + Handle(i)) - Handle(i);
			INT NewTotal = 0;
			Split(i)(0).Start = 0;
			for( INT j=0; j<Split(i).Num()-1; j++ )
			{
				if( Recompute )
					Split(i)(j).Size = (New(i)-Overhead) * Split(i)(j).Fraction;
				Split(i)(j+1).Start = Split(i)(j).Start + Split(i)(j).Size + 2*Border(i) + Handle(i);
				NewTotal += Split(i)(j).Size;
			}
			if( Recompute )
				Split(i).Last().Size = (New(i)-Overhead) - NewTotal;
		}
		for( i=0; i<SplitX.Num(); i++ )
		{
			for( INT j=0; j<SplitY.Num(); j++ )
			{
				WWindow* Window = Cells( i+j*SplitX.Num() );
				if( Window )
					Window->MoveWindow( GetSplitClientRect(i,j), 0 );
			}
		}
		InvalidateRect( hWnd, NULL, 0 );
		MoveWindow( FRect(FPoint(0,0),New), 1 );
		unguard;
	}
	void OnLeftButtonDown()
	{
		guard(WSplitter::OnLeftButtonDown);
		if( GetDragIndices()!=FPoint::NoneValue() )
		{
			DragInterceptor = new WDragInterceptor( this, GetDragIndices(), GetClientRect(), Border+Border+Handle );	
			for( INT i=0; i<FPoint::Num(); i++ )
			{
				if( DragInterceptor->DragIndices(i)!=INDEX_NONE )
				{
					DragInterceptor->DragPos(i)       = Split(i)(DragInterceptor->DragIndices(i)  ).Start + Split(i)(DragInterceptor->DragIndices(i)).Size + Border(i);
					DragInterceptor->DragClamp.Min(i) = Split(i)(DragInterceptor->DragIndices(i)  ).Start + Border(i);
					DragInterceptor->DragClamp.Max(i) = Split(i)(DragInterceptor->DragIndices(i)+1).Start + Split(i)(DragInterceptor->DragIndices(i)+1).Size - 2*Border(i);
				}
			}
			DragInterceptor->OpenWindow();
		}
		unguard;
	}
	void OnFinishSplitterDrag( WDragInterceptor* Drag, UBOOL Success )
	{
		guard(WSplitter::OnFinishSplitterDrag);
		if( Success )
		{
			FPoint Delta = Drag->DragPos - Drag->DragStart;
			for( INT i=0; i<2; i++ )
			{
				if( Drag->DragIndices(i)!=INDEX_NONE )
				{
					Split(i)(Drag->DragIndices(i)+0).Size += Delta(i);
					Split(i)(Drag->DragIndices(i)+1).Size -= Delta(i);
					INT Sum = 0;
					for( INT j=0; j<Split(i).Num(); j++ )
						Sum += Split(i)(j).Size;
					for( j=0; j<Split(i).Num(); j++ )
						Split(i)(j).Fraction = Split(i)(j).Size / (FLOAT)Sum;
				}
			}
			Resize( 0 );
		}
		DragInterceptor = NULL;
		unguard;
	}
	void OnPaint()
	{
		guard(WSplitter::OnPaint);
		if( GetUpdateRect( *this, NULL, 0 ) )
		{
			PAINTSTRUCT PS;
			HDC hDC = BeginPaint( *this, &PS );
			DrawCellFrames( hDC );
			EndPaint( *this, &PS );
		}
		unguard;
	}
	INT OnSetCursor()
	{
		guard(OnSetCursor::OnSetCursor);
		WWindow::OnSetCursor();
		FPoint P = GetDragIndices();
		if( P!=FPoint::NoneValue() )
		{
			if( P.X==INDEX_NONE )
				SetCursor(LoadCursorIdX(hInstanceWindow,IDC_SplitNS));
			else if( P.Y==INDEX_NONE )
				SetCursor(LoadCursorIdX(hInstanceWindow,IDC_SplitWE));
			else
				SetCursor(LoadCursorIdX(hInstanceWindow,IDC_SplitALL));
			return 1;
		}
		else return 0;
		unguard;
	}
};

/*-----------------------------------------------------------------------------
	WLevelFrame.
-----------------------------------------------------------------------------*/

// Test.
class WLevelFrame : public WWindow
{
	DECLARE_WINDOWCLASS(WLevelFrame,WWindow,Window)

	// Variables.
	ULevel* Level;
	WSplitter Splitter;

	// Structors.
	WLevelFrame( ULevel* InLevel, FName InPersistentName, WWindow* InOwnerWindow )
	:	WWindow( InPersistentName, InOwnerWindow )
	,	Level( InLevel )
	,	Splitter( this, 2, 2 )
	{}

	// WWindow interface.
	void OnKillFocus( HWND hWndNew )
	{
		GEditor->Client->MakeCurrent( NULL );
	}
	void Serialize( FArchive& Ar )
	{
		guard(WLevelFrame::Serialize);
		WWindow::Serialize( Ar );
		Ar << Level;
		unguard;
	}
	void OpenWindow( UBOOL bMdi, UBOOL bMax )
	{
		guard(WLevelFrame::OpenWindow);
		MdiChild = bMdi;
		PerformCreateWindowEx
		(
			MdiChild
			?	(WS_EX_MDICHILD)
			:	(0),
			TEXT("Level"),
			(bMax ? WS_MAXIMIZE : 0 ) |
			(MdiChild
			?	(WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_SYSMENU | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
			:	(WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS)),
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			512,
			384,
			MdiChild ? OwnerWindow->OwnerWindow->hWnd : OwnerWindow->hWnd,
			NULL,
			hInstance
		);
		Splitter.OpenWindow();
		if( !MdiChild )
		{
			SetWindowLongX( hWnd, GWL_STYLE, WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS );
			OwnerWindow->Show(1);
		}

		// Viewports.
		OpenFrameViewport( 0, 0, TEXT("Standard1V"), REN_OrthXY,   SHOW_Frame | SHOW_Actors | SHOW_Brush | SHOW_StandardView | SHOW_ChildWindow | SHOW_MovingBrushes );
		OpenFrameViewport( 1, 0, TEXT("Standard2V"), REN_OrthXZ,   SHOW_Frame | SHOW_Actors | SHOW_Brush | SHOW_StandardView | SHOW_ChildWindow | SHOW_MovingBrushes );
		OpenFrameViewport( 0, 1, TEXT("Standard3V"), REN_DynLight, SHOW_Menu | SHOW_Frame | SHOW_Actors | SHOW_Brush | SHOW_StandardView | SHOW_ChildWindow | SHOW_MovingBrushes );
		OpenFrameViewport( 1, 1, TEXT("Standard4V"), REN_OrthYZ,   SHOW_Frame | SHOW_Actors | SHOW_Brush | SHOW_StandardView | SHOW_ChildWindow | SHOW_MovingBrushes );

		unguard;
	}
	void OnSize( DWORD Flags, INT NewX, INT NewY )
	{
		guard(WLevelFrame::OnSize);
		WWindow::OnSize( Flags, NewX, NewY );
		if( Splitter.hWnd )
			Splitter.Resize( 1 );
		unguard;
	}
	INT OnSetCursor()
	{
		guard(WLevelFrame::OnSetCursor);
		WWindow::OnSetCursor();
		SetCursor(LoadCursorIdX(NULL,IDC_ARROW));
		return 0;
		unguard;
	}
	void OnPaint()
	{
		guard(WLevelFrame::OnPaint);
		PAINTSTRUCT PS;
		HDC hDC = BeginPaint( *this, &PS );
		FillRect( hDC, GetClientRect(), (HBRUSH)(COLOR_BTNFACE+1) );
		EndPaint( *this, &PS );
		unguard;
	}

	// WLevelFrame interface.
	virtual void OpenFrameViewport( INT X, INT Y, FName Name, INT RendMap, DWORD ShowFlags )
	{
		guard(WLevelFrame::OpenFrameViewport);

		UViewport* Viewport = GEditor->Client->NewViewport( Name );
		Level->SpawnViewActor( Viewport );
		Viewport->Actor->ShowFlags = ShowFlags;
		Viewport->Actor->RendMap   = RendMap;
		Viewport->Input->Init( Viewport );
		FRect R = Splitter.GetSplitClientRect( X, Y );
		Viewport->OpenWindow( (DWORD)Splitter.hWnd, 0, R.Width(), R.Height(), R.Min.X, R.Min.Y );
		for( INT i=0; i<_Windows.Num(); i++ )//!!
			if( Viewport->GetWindow()==_Windows(i)->hWnd )
				Splitter.Cells( X + Y*Splitter.SplitX.Num() ) = _Windows(i);

		unguard;
	}
};

/*-----------------------------------------------------------------------------
	WNewObject.
-----------------------------------------------------------------------------*/

// New object window.
class WNewObject : public WDialog
{
	DECLARE_WINDOWCLASS(WNewObject,WDialog,UnrealEd)

	// Variables.
	WButton OkButton;
	WButton CancelButton;
	WListBox TypeList;
	WObjectProperties Props;
	UObject* Context;
	UObject* Result;
 
	// Constructor.
	WNewObject( UObject* InContext, WWindow* InOwnerWindow )
	:	WDialog		( TEXT("NewObject"), IDDIALOG_NewObject, InOwnerWindow )
	,	OkButton    ( this, IDOK,     FDelegate(this,(TDelegate)OnOk) )
	,	CancelButton( this, IDCANCEL, FDelegate(this,(TDelegate)EndDialogFalse) )
	,	TypeList	( this, IDC_TypeList )
	,	Props		( NAME_None, CPF_Edit, TEXT(""), this, 0 )
	,	Context     ( InContext )
	,	Result		( NULL )
	{
		Props.ShowTreeLines = 0;
		TypeList.DoubleClickDelegate=FDelegate(this,(TDelegate)OnOk);
	}

	// WDialog interface.
	void OnInitDialog()
	{
		guard(WNewObject::OnInitDialog);
		WDialog::OnInitDialog();
		for( TObjectIterator<UClass> It; It; ++It )
		{
			if( It->IsChildOf(UFactory::StaticClass()) )
			{
				UFactory* Default = (UFactory*)It->GetDefaultObject();
				if( Default->bCreateNew )
					TypeList.SetItemData( TypeList.AddString( *Default->Description ), *It );
			}
		}
		Props.OpenChildWindow( IDC_PropHolder );
		TypeList.SetCurrent( 0, 1 );
		TypeList.SelectionChangeDelegate = FDelegate(this,(TDelegate)OnSelChange);
		OnSelChange();
		unguard;
	}
	void OnDestroy()
	{
		guard(WNewObject::OnDestroy);
		WDialog::OnDestroy();
		unguard;
	}
	virtual UObject* DoModal()
	{
		guard(WNewObject::DoModal);
		WDialog::DoModal( hInstance );
		return Result;
		unguard;
	}

	// Notifications.
	void OnSelChange()
	{
		guard(WNewObject::OnSelChange);
		INT Index = TypeList.GetCurrent();
		if( Index>=0 )
		{
			UClass*   Class   = (UClass*)TypeList.GetItemData(Index);
			UObject*  Factory = ConstructObject<UFactory>( Class );
			Props.Root.SetObjects( &Factory, 1 );
			EnableWindow( OkButton, 1 );
		}
		else
		{
			Props.Root.SetObjects( NULL, 0 );
			EnableWindow( OkButton, 0 );
		}
		unguard;
	}
	void OnOk()
	{
		guard(WNewObject::OnOk);
		if( Props.Root._Objects.Num() )
		{
			UFactory* Factory = CastChecked<UFactory>(Props.Root._Objects(0));
			Result = Factory->FactoryCreateNew( Factory->SupportedClass, NULL, NAME_None, 0, Context, GWarn );
			if( Result )
				EndDialogTrue();
		}
		unguard;
	}

	// WWindow interface.
	void Serialize( FArchive& Ar )
	{
		guard(WNewObject::Serialize);
		WDialog::Serialize( Ar );
		Ar << Context;
		for( INT i=0; i<TypeList.GetCount(); i++ )
		{
			UObject* Obj = (UClass*)TypeList.GetItemData(i);
			Ar << Obj;
		}
		unguard;
	}
};

/*-----------------------------------------------------------------------------
	WEditCode.
-----------------------------------------------------------------------------*/

// A code editing window.
class WCodeFrameBase : public WWindow
{
	DECLARE_WINDOWCLASS(WCodeFrameBase,WWindow,UnrealEd)

	// Constructor.
	WCodeFrameBase( FName InPersistentName, WWindow* InOwnerWindow )
	: WWindow( InPersistentName, InOwnerWindow )
	{}
};

// Code edit control.
class WEditCode : public WEdit
{
	DECLARE_WINDOWCLASS(WEditCode,WEdit,UnrealEd)

	// Constructor.
	WEditCode( WWindow* InOwner )
	: WEdit( InOwner )
	{}

	// WWindow interface.
	/*void OnChar( TCHAR Ch )
	{
		if( Ch!=0 )
		{
			throw TEXT("NoRoute");
		}
	}
	void OnRightButtonDown()
	{
		throw TEXT("NoRoute");
	}
	void OnPaste()
	{
		throw TEXT("NoRoute");
	}
	void OnUndo()
	{
		throw TEXT("NoRoute");
	}*/
};

// A code editing window.
class WCodeFrame : public WCodeFrameBase
{
	DECLARE_WINDOWCLASS(WCodeFrame,WCodeFrameBase,UnrealEd)

	// Variables.
	UClass* Class;
	WEditCode Edit;

	// Constructor.
	WCodeFrame( UClass* InClass, FName InPersistentName, WWindow* InOwnerWindow )
	: WCodeFrameBase( InPersistentName, InOwnerWindow )
	, Edit( this )
	, Class( InClass )
	{}

	// WWindow interface.
	void Serialize( FArchive& Ar )
	{
		guard(WCodeFrame::Serialize);
		WCodeFrameBase::Serialize( Ar );
		Ar << Class;
		unguard;
	}
	void OnSetFocus( HWND hWndLoser )
	{
		guard(WTerminal::OnSetFocus);
		WWindow::OnSetFocus( hWndLoser );
		SetFocus( Edit );
		unguard;
	}
	void OnSize( DWORD Flags, INT NewX, INT NewY )
	{
		guard(WTerminal::OnSize);
		WWindow::OnSize( Flags, NewX, NewY );
		Edit.MoveWindow( FRect(0,0,NewX,NewY), TRUE );
		Edit.ScrollCaret();
		unguard;
	}
	void OnCreate()
	{
		guard(WTerminal::OnCreate);
		WWindow::OnCreate();
		Edit.OpenWindow(1,1,0);
		DWORD Tabs[16];
		for( INT i=0; i<16; i++ )
			Tabs[i]=5*4*(i+1);
		SendMessage( Edit, EM_SETTABSTOPS, 16, (LPARAM)Tabs );
		Edit.SetFont( (HFONT)GetStockObject(ANSI_FIXED_FONT) );
		Edit.SetText( *Class->ScriptText->Text );
		unguard;
	}
	void OpenWindow( UBOOL bMdi=0, UBOOL AppWindow=0 )
	{
		guard(WTerminal::OpenWindow);
		MdiChild = bMdi;
		PerformCreateWindowEx
		(
			MdiChild
			?	(WS_EX_MDICHILD)
			:	(AppWindow?WS_EX_APPWINDOW:0),
			Class->GetFullName(),
			MdiChild
			?	(WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_SYSMENU | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
			:	(WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX),
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			384,
			512,
			MdiChild ? OwnerWindow->OwnerWindow->hWnd : OwnerWindow->hWnd,
			NULL,
			hInstance
		);
		unguard;
	}
};

/*-----------------------------------------------------------------------------
	WEditorFrame.
-----------------------------------------------------------------------------*/

// Editor frame window.
class WEditorFrame : public WMdiFrame, public FNotifyHook, public FDocumentManager
{
	DECLARE_WINDOWCLASS(WEditorFrame,WMdiFrame,UnrealEd)

	// Variables.
	WBackgroundHolder BackgroundHolder;
	WConfigProperties* Preferences;

	// Constructors.
	WEditorFrame()
	: WMdiFrame( TEXT("EditorFrame") )
	, BackgroundHolder( NAME_None, &MdiClient )
	, Preferences( NULL )
	{}

	// WWindow interface.
	void OnCreate()
	{
		guard(WEditorFrame::OnCreate);
		WMdiFrame::OnCreate();
		SetText( *FString::Printf( LocalizeGeneral(TEXT("FrameWindow"),TEXT("UnrealEd")), LocalizeGeneral(TEXT("Product"),TEXT("Core"))) );

		// Create MDI client.
		SetMenu( hWnd, LoadMenuIdX(hInstance,IDMENU_TempMenu) );
		CLIENTCREATESTRUCT ccs;
        ccs.hWindowMenu = GetSubMenu( GetMenu(hWnd), 6 ); 
        ccs.idFirstChild = 60000;
		MdiClient.OpenWindow( &ccs );

		// Background.
		BackgroundHolder.OpenWindow();

		unguard;
	}
	void RepositionClient()
	{
		guard(WEditorFrame::RepositionClient);
		WMdiFrame::RepositionClient();
		BackgroundHolder.MoveWindow( MdiClient.GetClientRect(), 1 );
		unguard;
	}
	void OnClose()
	{
		guard(WEditorFrame::OnClose);
		debugf( TEXT("WEditorFrame::OnClose") );
		appRequestExit( 0 );
		WMdiFrame::OnClose();
		unguard;
	}
	void OnCommand( INT Command )
	{
		guard(WEditorFrame::OnCommand);
		if( Command==IDC_FileNew )
		{
			WNewObject Dialog( NULL, this );
			UObject* Result = Dialog.DoModal();
			if( Cast<ULevel>(Result) )
				OpenLevelView();//!!manage better
		}
		else if( Command==ID_FileOpen )
		{
		}
		else if( Command==ID_FileClose )
		{
		}
		else if( Command==ID_FileSave )
		{
		}
		else if( Command==ID_FileSaveAs )
		{
		}
		else if( Command==ID_FileSaveAll )
		{
		}
		else if( Command==ID_FileExit )
		{
			OnClose();
		}
		else if( Command==ID_EditUndo )
		{
			GEditor->Exec( TEXT("TRANSACTION UNDO") );
		}
		else if( Command==ID_EditRedo )
		{
			GEditor->Exec( TEXT("TRANSACTION REDO") );
		}
		else if( Command==ID_EditDuplicate )
		{
			GEditor->Exec( TEXT("DUPLICATE") );
		}
		else if( Command==ID_EditDelete )
		{
			GEditor->Exec( TEXT("DELETE") );
		}
		else if( Command==ID_EditCut )
		{
			GEditor->Exec( TEXT("EDIT CUT") );
		}
		else if( Command==ID_EditCopy )
		{
			GEditor->Exec( TEXT("EDIT COPY") );
		}
		else if( Command==ID_EditPaste )
		{
			GEditor->Exec( TEXT("EDIT PASTE") );
		}
		else if( Command==ID_EditSelectNone )
		{
			GEditor->Exec( TEXT("SELECT NONE") );
		}
		else if( Command==ID_EditSelectAllActors )
		{
			GEditor->Exec( TEXT("ACTOR SELECT ALL") );
		}
		else if( Command==ID_EditSelectAllSurfs )
		{
			GEditor->Exec( TEXT("POLY SELECT ALL") );
		}
		else if( Command==ID_ViewActorProp )
		{
			if( !GEditor->ActorProperties )
			{
				GEditor->ActorProperties = new WObjectProperties( TEXT("ActorProperties"), CPF_Edit, TEXT(""), NULL, 1 );
				GEditor->ActorProperties->OpenWindow( hWnd );
				GEditor->ActorProperties->SetNotifyHook( GEditor );
			}
			GEditor->UpdatePropertiesWindows();
			GEditor->ActorProperties->Show(1);
		}
		else if( Command==ID_ViewSurfaceProp )
		{
		}
		else if( Command==ID_ViewLevelProp )
		{
			if( !GEditor->LevelProperties )
			{
				GEditor->LevelProperties = new WObjectProperties( TEXT("LevelProperties"), CPF_Edit, TEXT("Level Properties"), NULL, 1 );
				GEditor->LevelProperties->OpenWindow( hWnd );
				GEditor->LevelProperties->SetNotifyHook( GEditor );
			}
			GEditor->LevelProperties->Root.SetObjects( (UObject**)&GEditor->Level->Actors(0), 1 );
			GEditor->LevelProperties->Show(1);
		}
		else if( Command==ID_BuildGeometry )
		{
			GEditor->Exec( TEXT("MAP REBUILD") );
		}
		else if( Command==ID_BuildBSP )
		{
			GEditor->Exec( TEXT("BSP REBUILD") );
		}
		else if( Command==ID_BuildLighting )
		{
			GEditor->Exec( TEXT("LIGHT APPLY") );
		}
		else if( Command==ID_BuildRebuild )
		{
			GEditor->Exec( TEXT("MAP REBUILD") );
			GEditor->Exec( TEXT("BSP REBUILD") );
			GEditor->Exec( TEXT("LIGHT APPLY") );
		}
		else if( Command==ID_BuildPaths )
		{
			GEditor->Exec( TEXT("PATHS DEFINE") );
		}
		else if( Command==ID_ToolsLog )
		{
			if( GLogWindow )
			{
				GLogWindow->Show(1);
				SetFocus( *GLogWindow );
				GLogWindow->Display.ScrollCaret();
			}
		}
		else if( Command==ID_WindowCascade )
		{
			SendMessageX( MdiClient, WM_MDICASCADE, 0, 0 );
		}
		else if( Command==ID_ToolsPrefs )
		{
			if( !Preferences )
			{
				Preferences = new WConfigProperties( TEXT("Preferences"), LocalizeGeneral(TEXT("AdvancedOptionsTitle"),TEXT("Window")) );
				Preferences->OpenWindow( *this );
				Preferences->SetNotifyHook( this );
				Preferences->ForceRefresh();
			}
			Preferences->Show(1);
		}
		else if( Command==ID_WindowTileH )
		{
			SendMessageX( MdiClient, WM_MDITILE, MDITILE_HORIZONTAL, 0 );
		}
		else if( Command==ID_WindowTileV )
		{
			SendMessageX( MdiClient, WM_MDITILE, MDITILE_VERTICAL, 0 );
		}
		else WMdiFrame::OnCommand(Command);
		unguard;
	}
	void NotifyDestroy( void* Other )
	{
		if( Other==Preferences )
			Preferences=NULL;
	}

	// FDocumentManager interface.
	virtual void OpenLevelView()
	{
		guard(WEditorFrame::OpenLevelView);
		WLevelFrame* LevelFrame = new WLevelFrame( GEditor->Level, TEXT("LevelFrame"), &BackgroundHolder );
		LevelFrame->OpenWindow( 1, 1 );
		LevelFrame->Splitter.MdiClient = &MdiClient;//!!hacked
		unguard;
	}
	virtual void OpenScriptView( UClass* Class )
	{
		guard(WEditorFrame::OpenScriptView);
		WCodeFrame* CodeFrame = new WCodeFrame( Class, *(FString(TEXT("ScriptEd"))+Class->GetPathName()), &BackgroundHolder );
		CodeFrame->OpenWindow(1,0);
		CodeFrame->Show(1);
		unguard;
	}
};

/*-----------------------------------------------------------------------------
	Class browser.
-----------------------------------------------------------------------------*/

// An category header list item.
class FObjectBrowserItem : public FHeaderItem
{
public:
	// Variables.
	UObject* Object;

	// Constructors.
	FObjectBrowserItem( WPropertiesBase* InOwnerProperties, FTreeItem* InParent, UObject* InObject, UBOOL InExpandable )
	:	FHeaderItem( InOwnerProperties, InParent, InExpandable )
	,	Object( InObject )
	{}

	// FTreeItem interface.
	void Serialize( FArchive& Ar )
	{
		guard(FObjectBrowserItem::Serialize);
		FHeaderItem::Serialize( Ar );
		Ar << Object;
		unguard;
	}
	QWORD GetId() const
	{
		guard(FObjectBrowserItem::GetId);
		return Object->GetIndex() + ((QWORD)5<<32);
		unguard;
	}
	virtual FString GetCaption() const
	{
		guard(FObjectBrowserItem::GetText);
		return Object ? Object->GetName() : TEXT("");
		unguard;
	}
	void Collapse()
	{
		guard(FCategoryItem::Collapse);
		FTreeItem::Collapse();
		EmptyChildren();
		unguard;
	}
};

// An category header list item.
class FClassBrowserItem : public FObjectBrowserItem
{
public:
	FClassBrowserItem( WPropertiesBase* InOwnerProperties, FTreeItem* InParent, UClass* InClass )
	:	FObjectBrowserItem( InOwnerProperties, InParent, InClass, 0 )
	{
		for( TObjectIterator<UClass> It; It; ++It )
			if( It->GetSuperClass()==InClass )
				break;
		Expandable = It;
	}
	UClass* GetBrowseClass()
	{
		guard(FClassBrowserItem::GetBrowseClass);
		return Cast<UClass>( Object );
		unguard;
	}
	FString GetCaption() const
	{
		if( Object )
			return FObjectBrowserItem::GetCaption();
		return TEXT("Class Hierarchy");
	}
	void OnItemRightMouseDown( FPoint P )
	{
		// For now, edit script.
		UClass* Class = CastChecked<UClass>(Object);
		if( Class->ScriptText )
			GDocumentManager->OpenScriptView(Class);
	}
	void Expand()
	{
		guard(FClassBrowserItem::Expand);
		for( TObjectIterator<UClass> It; It; ++It )
			if( It->GetSuperClass()==GetBrowseClass() )
				Children.AddItem(new(TEXT("FClassBrowserItem"))FClassBrowserItem( OwnerProperties, this, *It ));
		FObjectBrowserItem::Expand();
		unguard;
	}
};

// Multiple selection object properties.
class WClassBrowser : public WProperties
{
	DECLARE_WINDOWCLASS(WClassBrowser,WProperties,Window)

	// Variables.
	FClassBrowserItem Root;

	// Structors.
	WClassBrowser( FName InPersistentName, WWindow* InOwnerWindow )
	:	WProperties	( InPersistentName, InOwnerWindow )
	,	Root		( this, NULL, NULL )
	{
		PropertiesWindows.RemoveItem( this );//!!hack while observer code is unimplemented
	}

	// WPropertiesBase interface.
	FTreeItem* GetRoot()
	{
		guard(WClassBrowser::GetRoot);
		return &Root;
		unguard;
	}
};

/*-----------------------------------------------------------------------------
	Package browser.
-----------------------------------------------------------------------------*/

// An category header list item.
class FPackageBrowserItem : public FObjectBrowserItem
{
public:
	// Variables.
	UClass* Filter;
	FPackageBrowserItem( WPropertiesBase* InOwnerProperties, FTreeItem* InParent, UObject* InObject, UClass* InFilter )
	:	FObjectBrowserItem ( InOwnerProperties, InParent, InObject, 0 )
	,	Filter( InFilter )
	{
		for( TObjectIterator<UObject> It; It; ++It )
			if( It->IsIn(Object) && It->IsA(Filter) )
				break;
		Expandable = It;
	}
	void Expand()
	{
		guard(FPackageBrowserItem::Expand);
		for( TObjectIterator<UObject> It; It; ++It )
		{
			if( It->IsA(Filter) && It->IsIn(Object) )
			{
				for( UObject* Real=*It; Real->GetOuter()!=Object; Real=Real->GetOuter() );
				for( INT i=0; i<Children.Num(); i++ )
					if( ((FPackageBrowserItem*)Children(i))->Object==Real )
						break;
				if( i==Children.Num() )
					Children.AddItem(new(TEXT("FPackageBrowserItem"))FPackageBrowserItem( OwnerProperties, this, Real, Filter ));
			}
		}
		FObjectBrowserItem::Expand();
		unguard;
	}
};

// Multiple selection object properties.
class WPackageBrowser : public WProperties
{
	DECLARE_WINDOWCLASS(WPackageBrowser,WProperties,Window)

	// Variables.
	FPackageBrowserItem Root;

	// Structors.
	WPackageBrowser( FName InPersistentName, WWindow* InOwnerWindow, UClass* InFilter )
	:	WProperties	( InPersistentName, InOwnerWindow )
	,	Root		( this, NULL, NULL, InFilter )
	{
		PropertiesWindows.RemoveItem( this );//!!hack while observer code is unimplemented
	}

	// WPropertiesBase interface.
	FTreeItem* GetRoot()
	{
		guard(WPackageBrowser::GetRoot);
		return &Root;
		unguard;
	}
};

/*-----------------------------------------------------------------------------
	WBrowserFrame.
-----------------------------------------------------------------------------*/

// Multiple selection object properties.
class WBrowserFrame : public WWindow
{
	DECLARE_WINDOWCLASS(WBrowserFrame,WWindow,Window)

	// Variables.
	WCoolButton  ComboLeft;
	WCoolButton  ComboRight;
	WCoolButton  ComboTear;
	WComboBox    Combo;
	WProperties* Current;

	// Structors.
	WBrowserFrame( FName InPersistentName, WWindow* InOwnerWindow )
	:	WWindow		( InPersistentName, InOwnerWindow )
	,	ComboLeft	( this, 0, FDelegate(this,(TDelegate)OnComboLeft  ) )
	,	ComboRight	( this, 0, FDelegate(this,(TDelegate)OnComboRight ) )
	,	ComboTear	( this, 0, FDelegate(this,(TDelegate)OnComboTear  ) )
	,	Combo		( this )
	,	Current		( NULL )
	{
		guard(WBrowserFrame::WBrowserFrame);
		unguard;
	}

	// Functions.
	void OnComboLeft()
	{
	}
	void OnComboRight()
	{
	}
	void OnComboTear()
	{
		guard(WBrowserFrame::OnComboTear);

		//!!
		//WProperties* P = new WClassBrowser( TEXT("ClassBrowser"), OwnerWindow->OwnerWindow );
		WProperties* P = new WPackageBrowser( TEXT("PackageBrowser"), OwnerWindow->OwnerWindow, UObject::StaticClass() );
		P->OpenWindow();
		P->ForceRefresh();
		P->Show(1);

		unguard;
	}
	void OnSize( DWORD Flags, INT X, INT Y )
	{
		guard(WBrowserFrame::OnSize);
		FRect R = GetClientRect();

		// Size the controls.
		ComboLeft .MoveWindow( FRect(2,2,14,21+2), 1 );
		ComboRight.MoveWindow( FRect(14,2,12+14,21+2), 1 );
		ComboTear .MoveWindow( FRect(R.Width()-12-2,2,R.Width()-2,21+2), 1 );
		Combo     .MoveWindow( FRect(12+14+2,2,R.Width()-12-2-2,21+2), 1 );

		// Size the sub-browser windows.
		if( Current )
			Current->MoveWindow( R.Bottom(R.Height()-23), 1 );

		unguard;
	}
	void OnPaint()
	{
		guard(WDockingFrame::OnPaint);
		PAINTSTRUCT PS;
		HDC hDC = BeginPaint( *this, &PS );

		FRect Rect = GetClientRect();
		FillRect( hDC, Rect, (HBRUSH)(COLOR_BTNFACE+1) );
		DrawEdge( hDC, Rect, BDR_RAISEDINNER, BF_TOPLEFT|BF_BOTTOMRIGHT );

		EndPaint( *this, &PS );
		unguard;
	}
	void OpenWindow()
	{
		guard(WBrowserFrame::OpenWindow);
		FRect R = OwnerWindow->GetClientRect();

		// Open the windows.
		PerformCreateWindowEx
		(
			0,
			NULL,
			WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE,
			0, 0, 0, 0,
			OwnerWindow->hWnd,
			NULL,
			hInstance
		);

		// Open the controls.
		ComboLeft .OpenWindow( 1, 0, 0, 0, 0, TEXT("<" ) );
		ComboRight.OpenWindow( 1, 0, 0, 0, 0, TEXT(">" ) );
		ComboTear .OpenWindow( 1, 0, 0, 0, 0, TEXT("..") );
		Combo     .OpenWindow( 1 );

		// Init the combo.!!
		Combo.AddString( TEXT("Class Hierarchy") );
		Combo.AddString( TEXT("Classes") );
		Combo.AddString( TEXT("Textures") );
		Combo.AddString( TEXT("Sounds") );
		Combo.AddString( TEXT("Actors by class") );
		Combo.AddString( TEXT("Actors by tag") );
		Combo.AddString( TEXT("Actors by event") );
		Combo.AddString( TEXT("Music") );
		Combo.AddString( TEXT("Meshes") );
		Combo.AddString( TEXT("Fonts") );
		Combo.AddString( TEXT("Brushes") );
		Combo.AddString( TEXT("All Objects") );
		Combo.SetCurrent( 0 );

		// Open current browser page.!!
		Current = new WClassBrowser( NAME_None, this );
		Current->OpenChildWindow( 0 );
		Current->ForceRefresh();
		MoveWindow( OwnerWindow->GetClientRect(), 1 );
		unguard;
	}
};

/*-----------------------------------------------------------------------------
	WToolbar.
-----------------------------------------------------------------------------*/

// An category header list item.
class FToolbarItem : public FHeaderItem
{
public:
	// Variables.
	FString Caption;

	// Constructors.
	FToolbarItem( WPropertiesBase* InOwnerProperties, FTreeItem* InParent, UBOOL InExpandable, const TCHAR* InCaption )
	:	FHeaderItem( InOwnerProperties, InParent, InExpandable )
	,	Caption( InCaption )
	{
		Sorted = 0;
	}

	// FTreeItem interface.
	INT GetIndentPixels( UBOOL Text )
	{
		return Expandable ? FHeaderItem::GetIndentPixels(Text) : 16;
	}
	UBOOL GetSelected()
	{
		return 0;
	}
	HBRUSH GetBackgroundBrush( UBOOL Selected )
	{
		guard(FPropertyItem::GetBackgroundBrush);
		return Selected ? hBrushCurrent : Children.Num() ? hBrushHeadline : hBrushOffWhite;
		unguard;
	}
	QWORD GetId() const
	{
		guard(FObjectBrowserItem::GetId);
		return appStrCrc( *Caption );
		unguard;
	}
	virtual FString GetCaption() const
	{
		guard(FObjectBrowserItem::GetText);
		return Caption;
		unguard;
	}
};

// An editor mode.
class FToolbarModeItem : public FToolbarItem
{
public:
	EEditorMode Mode;
	FToolbarModeItem( WPropertiesBase* InOwnerProperties, FTreeItem* InParent, UBOOL InExpandable, const TCHAR* InCaption, EEditorMode InMode )
	:	FToolbarItem( InOwnerProperties, InParent, InExpandable, InCaption )
	,	Mode( InMode )
	{}
	UBOOL GetSelected()
	{
		return GEditor->Mode==Mode;
	}
	HBRUSH GetBackgroundBrush( UBOOL Selected )
	{
		guard(FPropertyItem::GetBackgroundBrush);
		return Selected ? hBrushMode : hBrushOffWhite;
		unguard;
	}
	void OnItemLeftMouseDown( FPoint P )
	{
		GEditor->edcamSetMode( Mode );
		GEditor->RedrawLevel( GEditor->Level );
		InvalidateRect( OwnerProperties->List, NULL, 0 );
		UpdateWindow( OwnerProperties->List );
	}
};

// A brush builder.
class FToolbarBrushItem : public FObjectsItem
{
public:
	UBrushBuilder* Builder;
	FToolbarBrushItem( WPropertiesBase* InOwnerProperties, FTreeItem* InParent, UClass* Class )
	:	FObjectsItem( InOwnerProperties, InParent, CPF_Edit, Class->GetName(), 0 )
	,	Builder(NULL)
	{
		guard(FToolbarBrushItem::FToolbarBrushItem);
		Builder = ConstructObject<UBrushBuilder>(Class);
		SetObjects( (UObject**)&Builder, 1 );
		Sorted = 0;
		unguard;
	}
	void OnBuild()
	{
		guard(FToolbarBrushItem::OnBuild);
		OwnerProperties->SetItemFocus(0);
		UBOOL GIsSavedScriptableSaved = 1;
		Exchange(GIsScriptable,GIsSavedScriptableSaved);
		Builder->eventBuild();
		Exchange(GIsScriptable,GIsSavedScriptableSaved);
		unguard;
	}
	void OnItemSetFocus()
	{
		FObjectsItem::OnItemSetFocus();
		AddButton( TEXT("Build"), FDelegate(this,(TDelegate)OnBuild) );
	}
	void Serialize( FArchive& Ar )
	{
		guard(FPropertyItemBase::Serialize);
		FObjectsItem::Serialize( Ar );
		Ar << Builder;
		unguard;
	}
};

// An editor command.
class FToolbarCommandItem : public FToolbarItem
{
public:
	FString Cmd;
	UBOOL Executing;
	FToolbarCommandItem( WPropertiesBase* InOwnerProperties, FTreeItem* InParent, UBOOL InExpandable, const TCHAR* InCaption, const TCHAR* InCmd )
	:	FToolbarItem( InOwnerProperties, InParent, InExpandable, InCaption )
	,	Cmd( InCmd )
	,	Executing( 0 )
	{}
	HBRUSH GetBackgroundBrush( UBOOL Selected )
	{
		guard(FPropertyItem::GetBackgroundBrush);
		return Executing ? hBrushCurrent : hBrushOffWhite;
		unguard;
	}
	void OnItemLeftMouseDown( FPoint P )
	{
		Executing=1;
		InvalidateRect( OwnerProperties->List, NULL, 0 );
		UpdateWindow( OwnerProperties->List );
		GEditor->Exec( *Cmd );
		GEditor->RedrawLevel( GEditor->Level );
		Executing=0;
		InvalidateRect( OwnerProperties->List, NULL, 0 );
		UpdateWindow( OwnerProperties->List );
	}
};

// An category header list item.
class FToolbarRootItem : public FToolbarItem
{
public:
	// Variables.
	FString Caption;

	// Constructors.
	FToolbarRootItem( WPropertiesBase* InOwnerProperties, FTreeItem* InParent, UBOOL InExpandable, const TCHAR* InCaption )
	:	FToolbarItem( InOwnerProperties, InParent, InExpandable, InCaption )
	,	Caption( TEXT("Toolbar") )
	{}

	// FTreeItem interface.
	void Expand()
	{
		guard(FToolbarItem::Expand);
		FToolbarItem* T;
		Children.AddItem(T=new FToolbarItem(OwnerProperties,this,1,TEXT("Modes")));
			T->Children.AddItem(new FToolbarModeItem(OwnerProperties,T,0,TEXT("Camera Move"),EM_ViewportMove));
			T->Children.AddItem(new FToolbarModeItem(OwnerProperties,T,0,TEXT("Camera Zoom"),EM_ViewportZoom));
			T->Children.AddItem(new FToolbarModeItem(OwnerProperties,T,0,TEXT("Brush Rotate"),EM_BrushRotate));
			T->Children.AddItem(new FToolbarModeItem(OwnerProperties,T,0,TEXT("Brush Sheer"),EM_BrushSheer));
			T->Children.AddItem(new FToolbarModeItem(OwnerProperties,T,0,TEXT("Brush Scale Uniform"),EM_BrushScale));
			T->Children.AddItem(new FToolbarModeItem(OwnerProperties,T,0,TEXT("Brush Scale Axial"),EM_BrushStretch));
			T->Children.AddItem(new FToolbarModeItem(OwnerProperties,T,0,TEXT("Brush Scale Snap"),EM_BrushSnap));
			T->Children.AddItem(new FToolbarModeItem(OwnerProperties,T,0,TEXT("Pan Textures"),EM_TexturePan));
			T->Children.AddItem(new FToolbarModeItem(OwnerProperties,T,0,TEXT("Rotate Textures"),EM_TextureRotate));
		Children.AddItem(T=new FToolbarItem(OwnerProperties,this,1,TEXT("Brush Operations")));
			T->Children.AddItem(new FToolbarCommandItem(OwnerProperties,T,0,TEXT("Add"),TEXT("BRUSH ADD")));
			T->Children.AddItem(new FToolbarCommandItem(OwnerProperties,T,0,TEXT("Subtract"),TEXT("BRUSH SUBTRACT")));
			T->Children.AddItem(new FToolbarCommandItem(OwnerProperties,T,0,TEXT("Intersect"),TEXT("BRUSH FROM INTERSECTION")));
			T->Children.AddItem(new FToolbarCommandItem(OwnerProperties,T,0,TEXT("Deintersect"),TEXT("BRUSH FROM DEINTERSECTION")));
			T->Children.AddItem(new FToolbarCommandItem(OwnerProperties,T,0,TEXT("Add Mover"),TEXT("BRUSH ADDMOVER")));
			T->Children.AddItem(new FToolbarCommandItem(OwnerProperties,T,0,TEXT("Add Special..."),TEXT("")));
		Children.AddItem(T=new FToolbarItem(OwnerProperties,this,1,TEXT("Brush Factories")));
			for( TObjectIterator<UClass> ItC; ItC; ++ItC )
				if( ItC->IsChildOf(UBrushBuilder::StaticClass()) && !(ItC->ClassFlags&CLASS_Abstract) )
					T->Children.AddItem(new FToolbarBrushItem(OwnerProperties,T,*ItC));
		Children.AddItem(T=new FToolbarItem(OwnerProperties,this,1,TEXT("Select")));
			T->Children.AddItem(new FToolbarCommandItem(OwnerProperties,T,0,TEXT("All Actors"),TEXT("ACTOR SELECT ALL")));
			T->Children.AddItem(new FToolbarCommandItem(OwnerProperties,T,0,TEXT("All Surfaces"),TEXT("POLY SELECT ALL")));
			T->Children.AddItem(new FToolbarCommandItem(OwnerProperties,T,0,TEXT("Actors Inside Brush"),TEXT("ACTOR SELECT INSIDE")));
			T->Children.AddItem(new FToolbarCommandItem(OwnerProperties,T,0,TEXT("Actors Invert"),TEXT("ACTOR SELECT INVERT")));
			T->Children.AddItem(new FToolbarCommandItem(OwnerProperties,T,0,TEXT("None"),TEXT("SELECT NONE")));
		Children.AddItem(T=new FToolbarItem(OwnerProperties,this,1,TEXT("Tools")));
			T->Children.AddItem(new FToolbarCommandItem(OwnerProperties,T,0,TEXT("Replace Sel Brushes"),TEXT("ACTOR REPLACE BRUSH")));
			T->Children.AddItem(new FToolbarCommandItem(OwnerProperties,T,0,TEXT("Replace Sel Actors"),TEXT("ACTOR REPLACE CLASS=Light")));//!!
			T->Children.AddItem(new FToolbarCommandItem(OwnerProperties,T,0,TEXT("Toggle Vertex Snap"),TEXT("MODE SNAPVERTEX=off")));//!!
			T->Children.AddItem(new FToolbarCommandItem(OwnerProperties,T,0,TEXT("Camera Speed"),TEXT("MODE SPEED=16")));//!!
			T->Children.AddItem(new FToolbarItem(OwnerProperties,T,0,TEXT("Grid...")));
			T->Children.AddItem(new FToolbarItem(OwnerProperties,T,0,TEXT("Rotation Grid...")));
		Children.AddItem(T=new FToolbarItem(OwnerProperties,this,1,TEXT("Visibility")));
			T->Children.AddItem(new FToolbarCommandItem(OwnerProperties,T,0,TEXT("Show All"),TEXT("ACTOR UNHIDE ALL")));
			T->Children.AddItem(new FToolbarCommandItem(OwnerProperties,T,0,TEXT("Show Sel Actors"),TEXT("ACTOR HIDE UNSELECTED")));
			T->Children.AddItem(new FToolbarCommandItem(OwnerProperties,T,0,TEXT("Hide Sel Actors"),TEXT("ACTOR HIDE SELECTED")));
			T->Children.AddItem(new FToolbarCommandItem(OwnerProperties,T,0,TEXT("Toggle Z Region"),TEXT("ACTOR CLIP Z")));
		FToolbarItem::Expand();
		Children(0)->Expand();
		Children(1)->Expand();
		Children(2)->Expand();
		unguard;
	}
};

// Multiple selection object properties.
class WToolbar : public WProperties
{
	DECLARE_WINDOWCLASS(WToolbar,WProperties,Window)

	// Variables.
	FToolbarRootItem Root;

	// Structors.
	WToolbar( FName InPersistentName, WWindow* InOwnerWindow )
	:	WProperties	( NAME_None/*InPersistentName!!*/, InOwnerWindow )
	,	Root		( this, NULL, 1, TEXT("Toolbar") )
	{
		ShowTreeLines=0;
		PropertiesWindows.RemoveItem( this );//!!hack while observer code is unimplemented
	}

	// WPropertiesBase interface.
	FTreeItem* GetRoot()
	{
		return &Root;
	}
};

/*-----------------------------------------------------------------------------
	WinMain.
-----------------------------------------------------------------------------*/

//
// Main window entry point.
//
INT WINAPI WinMain( HINSTANCE hInInstance, HINSTANCE hPrevInstance, char* InCmdLine, INT nCmdShow )
{
	// Remember instance.
	GIsStarted = 1;
	hInstance = hInInstance;

	// Set package name.
	appStrcpy( GPackage, appPackage() );

	// Begin.
#ifndef _DEBUG
	try
	{
#endif
		// Start main loop.
		GIsGuarded=1;
		appInit( TEXT("Unreal"), GetCommandLine(), &Malloc, &Log, &Error, &Warn, &FileManager, FConfigCacheIni::Factory, 1 );

		// Init mode.
		GIsClient = GIsServer = GIsEditor = GLazyLoad = 1;
		GIsScriptable = 0;

		// Init windowing.
		InitWindowing();
		IMPLEMENT_WINDOWCLASS(WMdiFrame,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WEditorFrame,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WBackgroundHolder,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WSplitter,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WLevelFrame,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WDockingFrame,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WClassBrowser,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WPackageBrowser,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WToolbar,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WBrowserFrame,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WCodeFrameBase,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WCodeFrame,CS_DBLCLKS);
		IMPLEMENT_WINDOWSUBCLASS(WMdiClient,TEXT("MDICLIENT"));
		IMPLEMENT_WINDOWSUBCLASS(WEditCode,TEXT("EDIT"));

		// Windows.
		WEditorFrame Frame;
		GDocumentManager = &Frame;
		Frame.OpenWindow();
		InvalidateRect( Frame, NULL, 1 );
		UpdateWindow( Frame );
		UBOOL ShowLog = ParseParam(appCmdLine(),TEXT("log"));
		if( !ShowLog && !ParseParam(appCmdLine(),TEXT("server")) )
			InitSplash( TEXT("..\\Help\\Logo.bmp") );
		GLogWindow = new WLog( Log.Filename, Log.LogAr, TEXT("EditorLog"), &Frame );
		GLogWindow->OpenWindow( ShowLog, 0 );
		GEditor = CastChecked<UEditorEngine>(InitEngine());

		// Toolbar.
		WProperties* T = new WToolbar( TEXT("EditorToolbar"), &Frame.LeftFrame );
		T->OpenChildWindow( 0 );
		T->ForceRefresh();
		Frame.LeftFrame.Dock( T );

		// Browser.
		WBrowserFrame* B = new WBrowserFrame( TEXT("BrowserFrame"), &Frame.RightFrame );
		B->OpenWindow();
		Frame.RightFrame.Dock( B );

		ExitSplash();
		if( !GIsRequestingExit )
			MainLoop( GEditor );
		GDocumentManager=NULL;
		GFileManager->Delete(TEXT("Running.ini"),0,0);
		if( GLogWindow )
			delete GLogWindow;
		appPreExit();
		GIsGuarded = 0;
#ifndef _DEBUG
	}
	catch( ... )
	{
		// Crashed.
		Error.HandleError();
	}
#endif

	// Shut down.
	appExit();
	GIsStarted = 0;
	return 0;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
