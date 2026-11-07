// Container.h: CipSoft's container templates (c:\mud\tibiaplayer\Container.h).
//
// Every member here is written once and compiles to all of the binary's
// instances listed below (issue #13: each checked against its disassembly and
// exception table). The assertion texts and line numbers come from the
// instances; member names other than space and block are ours.
//
// The original file's growable vector<T> (lines 353-370) is TIndexedVector<T> of
// src/types/IndexedVector.h since Patella-Modern issue #99, the same semantics
// over a std::vector; its instances in the binary are listed here, by the
// original's name.
//
// The linker keeps one copy of an instance, normally in the first object that
// uses it (objects are in alphabetical order), so most copies sit in an
// object before the file that the instance is named after. Identical code of
// different T is not folded when the functions have exception tables (the
// constructors, one copy per object); the operator() of every vector of a
// 4-byte element is the one copy 0x452ac0. Instances, by element type:
//   vector<TLineFormat>     constructor 0x417aa0, operator() 0x417710
//                           (ConsoleWindow.h's TTextWindow::Lines, 0x28 bytes;
//                           new[] and init run TLineFormat's inline constructor)
//   vector<TSkinButton *>   initialized constructor 0x425e70 (DialogElements.h's
//                           TRadioButtonGroup, inline constructor in ContentWindows.obj)
//   vector<TMark>           constructor 0x426170, operator() 0x425b20,
//                           destructor 0x425b10 (Map.h's TAutomapBlock and the
//                           automap windows of ContentWindows.cpp; kept in
//                           ContentWindows.obj, before Map.obj)
//   vector<int>             initialized constructor 0x453300, constructor
//                           0x4a2b90, operator() 0x452ac0, destructor 0x452c50
//                           (DialogElements.h's TEditText line starts,
//                           Help.cpp's EventHelps, Preferences.cpp's lists;
//                           0x452ac0 and 0x452c50 also serve vector<TEditLine *>,
//                           vector<char *>, vector<TTimer *> and vector<TTalkFilter *>)
//   vector<TListBoxLine>    constructor 0x453600, operator() 0x452eb0,
//                           destructor 0x452c60 (DialogElements.h's TFontListBox
//                           lines, 0x18 bytes, the implicit element constructor
//                           0x452c70)
//   vector<TIgnoreEntry>    constructor 0x46c0d0, operator() 0x46bb70,
//                           destructor 0x46bb60 (Dialogs.h's 31-byte ignore
//                           list entries, with an inline constructor)
//   vector<THotkey>         constructor 0x4976f0, operator() 0x496570 (GUI.cpp's
//                           Hotkeys; new[] and init run TCallback's inline constructor)
//   vector<TTimer *>        initialized constructor 0x4973f0 (GUI.cpp's Timers)
//   vector<TContentWindowSize>  constructor 0x4dbe90, operator() 0x4db880 (Preferences.cpp)
//   vector<TBuddy>          constructor 0x4dc170, operator() 0x4dba60 (Preferences.cpp)
//   vector<TMessage>        constructor 0x4e7f50, operator() 0x4e7390 (Status.cpp)
//   vector<TErrorReport>    constructor 0x4e8230, operator() 0x4e7860 (Status.cpp until issue #74; the golden
//                           test keeps comparing it with a record of TErrorReport's size)
//   vector<TTalkFilter *>   initialized constructor 0x4fd6e0 (Talk.cpp's lists)
// The four initialized constructors (0x425e70, 0x453300, 0x4973f0, 0x4fd6e0)
// are the same code apart from addresses. The destructor is inline; its
// out-of-line copies are called from unwind funclets and atexit thunks, the
// other static vectors' atexit handlers (0x55a320 and following) free entry
// inline.

#pragma once

#include "common.h"
#include "compat/mdspan.h"
#include <memory>
#include <sstream>

// Each container owns its entries as a std::unique_ptr<T[]> (issue #58): a pointer at the original's offset, freed
// with reset() where the original had delete[] (the destructors), allocated with reset(new T[n])
// (the same new-expression: entries of a type without a constructor stay uninitialised). The element types'
// destructors cannot throw (scalars, pointers and plain structures), so neither can the reset.
static_assert(sizeof(std::unique_ptr<int[]>) == sizeof(int *) && alignof(std::unique_ptr<int[]>) == alignof(int *),
	"std::unique_ptr<T[]> is a pointer");

// array<T> is a fixed array indexed from min to min+d-1 (lines 152-178;
// it comes before vector in the original file). The class name is ours;
// min, d, i, low() and high() are named in the assertions and in the
// error message of operator(). Instances:
//   array<TProperties>  constructor 0x4cfa30, operator() 0x4d1a10 (Objects.cpp)
//   array<TMapPoint>    constructor 0x4b0bc0, operator() 0x4b5900 (Map.cpp)
template <class T> class array {
public:
	array(const array &) = delete;
	array &operator=(const array &) = delete;
	array(int Min, int Max);
	// Inlined into Objects.cpp's ReleaseObjects (delete[] entry, then
	// operator delete on the array itself).
	~array() { entry.reset(); }
	T &operator()(int i);
	int low() { return min; }
	int high() { return min + d - 1; }

	int min;                    // +0x00
	int d;                      // +0x04, number of entries
	std::unique_ptr<T[]> entry; // +0x08
};

// Line 152. No try block; the assertion is the only EH state.
template <class T> array<T>::array(int Min, int Max)
{
	min = Min;
	d = Max - Min + 1;
	PATELLA_ASSERT_VALUES(d>0, d);
	entry.reset(new T[d]);
}

// Line 178. The error has no try block either; its message is built in a
// stringstream.
template <class T> T &array<T>::operator()(int i)
{
	i -= min;
	if (i < 0 || i >= d) {
		std::stringstream s;
		s << "index out of range (" << "original i: " << i + min << ", i: " << i << ", min: " << min
		  << ", d: " << d << ")";
		TERROR(s.str().c_str());
	}
	return entry[i];
}

// matrix<T> is a fixed two-dimensional array indexed from xmin/ymin (lines
// 217-253); matrix3d<T> its three-dimensional counterpart (lines 282-328).
// The class names are ours; xmin, ymin, zmin, dx, dy, dz, x, y and z are named
// in the assertions and error messages. x is the fastest index. The
// destructors free the entries; their only out-of-line copy is matrix's at
// 0x4b0b20, called when an exception unwinds Map.cpp's FindPath.
// Instances (Map.cpp):
//   matrix<TPathNode>  constructor 0x4b1100, operator() 0x4b5e90, destructor 0x4b0b20
//   matrix3d<int>      constructor 0x4b0d70, operator() 0x4b5b40
//
// The entries are indexed through a std::mdspan view over the owned storage since Patella-Modern issue #101
// (src/compat/mdspan.h; notes/decisions.md, "#101: the matrices and the automap layers as mdspan views"): view()
// makes one from entry and the extents, layout_left, so view()[x, y] is entry[y * dx + x] and view()[x, y, z]
// entry[(z * dy + y) * dx + x] as in the original. operator() checks the indices first; a view does not. The members
// before entry and entry itself stay as they were (the golden test compares them with the original's).
template <class T> class matrix {
public:
	matrix(const matrix &) = delete;
	matrix &operator=(const matrix &) = delete;
	matrix(int XMin, int XMax, int YMin, int YMax);
	~matrix() { entry.reset(); }
	T &operator()(int x, int y);

	// [x, y] from 0, x the fastest index.
	using TView = patella::TMdspan<T, patella::TDynamicExtents<int, 2>, patella::TLayoutLeft>;
	using TConstView = patella::TMdspan<const T, patella::TDynamicExtents<int, 2>, patella::TLayoutLeft>;
	TView view() { return TView(entry.get(), dx, dy); }
	[[nodiscard]] TConstView view() const { return TConstView(entry.get(), dx, dy); }

	int xmin;                   // +0x00
	int ymin;                   // +0x04
	int dx;                     // +0x08
	int dy;                     // +0x0c
	std::unique_ptr<T[]> entry; // +0x10
};

// Lines 217-218. No try block; the assertions are the only EH states.
template <class T> matrix<T>::matrix(int XMin, int XMax, int YMin, int YMax)
{
	xmin = XMin;
	ymin = YMin;
	dx = XMax - XMin + 1;
	dy = YMax - YMin + 1;
	PATELLA_ASSERT_VALUES(dx>0, dx);
	PATELLA_ASSERT_VALUES(dy>0, dy);
	entry.reset(new T[dx * dy]);
}

// Line 253.
template <class T> T &matrix<T>::operator()(int x, int y)
{
	x -= xmin;
	y -= ymin;
	if (x < 0 || x >= dx || y < 0 || y >= dy) {
		std::stringstream s;
		s << "index out of range (" << "original x: " << x + xmin << ", original y: " << y + ymin
		  << ", x: " << x << ", y: " << y << ", xmin: " << xmin << ", ymin: " << ymin << ", dx: " << dx
		  << ", dy: " << dy << ")";
		TERROR(s.str().c_str());
	}
	return view()[x, y];
}

template <class T> class matrix3d {
public:
	matrix3d(const matrix3d &) = delete;
	matrix3d &operator=(const matrix3d &) = delete;
	matrix3d(int XMin, int XMax, int YMin, int YMax, int ZMin, int ZMax);
	~matrix3d() { entry.reset(); }
	T &operator()(int x, int y, int z);

	// [x, y, z] from 0, x the fastest index, z the slowest.
	using TView = patella::TMdspan<T, patella::TDynamicExtents<int, 3>, patella::TLayoutLeft>;
	using TConstView = patella::TMdspan<const T, patella::TDynamicExtents<int, 3>, patella::TLayoutLeft>;
	TView view() { return TView(entry.get(), dx, dy, dz); }
	[[nodiscard]] TConstView view() const { return TConstView(entry.get(), dx, dy, dz); }

	int xmin;                   // +0x00
	int ymin;                   // +0x04
	int zmin;                   // +0x08
	int dx;                     // +0x0c
	int dy;                     // +0x10
	int dz;                     // +0x14
	std::unique_ptr<T[]> entry; // +0x18
};

// Lines 282-284.
template <class T> matrix3d<T>::matrix3d(int XMin, int XMax, int YMin, int YMax, int ZMin, int ZMax)
{
	xmin = XMin;
	ymin = YMin;
	zmin = ZMin;
	dx = XMax - XMin + 1;
	dy = YMax - YMin + 1;
	dz = ZMax - ZMin + 1;
	PATELLA_ASSERT_VALUES(dx>0, dx);
	PATELLA_ASSERT_VALUES(dy>0, dy);
	PATELLA_ASSERT_VALUES(dz>0, dz);
	entry.reset(new T[dx * dy * dz]);
}

// Line 328. Unlike the other containers, the error names a bug number as its
// expression.
template <class T> T &matrix3d<T>::operator()(int x, int y, int z)
{
	x -= xmin;
	y -= ymin;
	z -= zmin;
	if (x < 0 || x >= dx || y < 0 || y >= dy || z < 0 || z >= dz) {
		std::stringstream s;
		s << "index out of range (" << "original x: " << x + xmin << ", original y: " << y + ymin
		  << ", original z: " << z + zmin << ", x: " << x << ", y: " << y << ", z: " << z
		  << ", xmin: " << xmin << ", ymin: " << ymin << ", zmin: " << zmin << ", dx: " << dx
		  << ", dy: " << dy << ", dz: " << dz << ")";
		throw TMYEXCEPTION(s.str().c_str(), "", 0, "", 0, "[bug0000013]");
	}
	return view()[x, y, z];
}
