//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2020, Cinesite VFX Ltd. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are
//  met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//
//     * Neither the name of Image Engine Design nor the names of any
//       other contributors to this software may be used to endorse or
//       promote products derived from this software without specific prior
//       written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
//  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
//  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
//  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
//  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
//  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////

#include "IECoreUSD/DataAlgo.h"
#include "IECoreUSD/ObjectAlgo.h"
#include "IECoreUSD/PrimitiveAlgo.h"

#include "IECoreScene/MeshAlgo.h"
#include "IECoreScene/MeshPrimitive.h"

IECORE_PUSH_DEFAULT_VISIBILITY
#include "pxr/usd/usdGeom/mesh.h"
IECORE_POP_DEFAULT_VISIBILITY

using namespace IECore;
using namespace IECoreScene;
using namespace IECoreUSD;

//////////////////////////////////////////////////////////////////////////
// Reading
//////////////////////////////////////////////////////////////////////////


// Some quick code from online to measure memory usage

/*
 * Author:  David Robert Nadeau
 * Site:    http://NadeauSoftware.com/
 * License: Creative Commons Attribution 3.0 Unported License
 *          http://creativecommons.org/licenses/by/3.0/deed.en_US
 * Available here : https://stackoverflow.com/a/14927379
 */

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>

#elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
#include <unistd.h>
#include <sys/resource.h>

#if defined(__APPLE__) && defined(__MACH__)
#include <mach/mach.h>

#elif (defined(_AIX) || defined(__TOS__AIX__)) || (defined(__sun__) || defined(__sun) || defined(sun) && (defined(__SVR4) || defined(__svr4__)))
#include <fcntl.h>
#include <procfs.h>

#elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
#include <stdio.h>

#endif

#else
#error "Cannot define getPeakRSS( ) or getCurrentRSS( ) for an unknown OS."
#endif


namespace
{

/**
 * Returns the current resident set size (physical memory use) measured
 * in bytes, or zero if the value cannot be determined on this OS.
 */
size_t getCurrentRSS( )
{
#if defined(_WIN32)
    /* Windows -------------------------------------------------- */
    PROCESS_MEMORY_COUNTERS info;
    GetProcessMemoryInfo( GetCurrentProcess( ), &info, sizeof(info) );
    return (size_t)info.WorkingSetSize;

#elif defined(__APPLE__) && defined(__MACH__)
    /* OSX ------------------------------------------------------ */
    struct mach_task_basic_info info;
    mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
    if ( task_info( mach_task_self( ), MACH_TASK_BASIC_INFO,
        (task_info_t)&info, &infoCount ) != KERN_SUCCESS )
        return (size_t)0L;      /* Can't access? */
    return (size_t)info.resident_size;

#elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
    /* Linux ---------------------------------------------------- */
    long rss = 0L;
    FILE* fp = NULL;
    if ( (fp = fopen( "/proc/self/statm", "r" )) == NULL )
        return (size_t)0L;      /* Can't open? */
    if ( fscanf( fp, "%*s%ld", &rss ) != 1 )
    {
        fclose( fp );
        return (size_t)0L;      /* Can't read? */
    }
    fclose( fp );
    return (size_t)rss * (size_t)sysconf( _SC_PAGESIZE);

#else
    /* AIX, BSD, Solaris, and Unknown OS ------------------------ */
    return (size_t)0L;          /* Unsupported. */
#endif
}

IECore::ObjectPtr readMesh( pxr::UsdGeomMesh &mesh, pxr::UsdTimeCode time, const Canceller *canceller )
{
	std::cerr << "Initial memory "  << getCurrentRSS() << "\n";
    {
        const pxr::UsdGeomPrimvar nPrimVar = mesh.GetPrimvar( pxr::TfToken( "normals" ) );
        pxr::VtValue nValue;
        nPrimVar.Get( &nValue, time );

		std::cerr << "Got VtValue for normals but not yet read "  << getCurrentRSS() << "\n";
		auto nArray = nValue.Get<pxr::VtArray<pxr::GfVec3f>>();
		const Imath::V3f *nArrayTyped = reinterpret_cast<const Imath::V3f *>( nArray.cdata() );
		
		Imath::V3f accum( 0 );
		for( unsigned int i = 0; i < nArray.size(); i++ )
		{
			accum += nArrayTyped[i];
		}
		std::cerr << "ACCUM normals : " << accum << "\n";
		std::cerr << "Memory after accumulating normals "  << getCurrentRSS() << "\n";
    }
	std::cerr << "Memory after releasing normals "  << getCurrentRSS() << "\n";

    {
        const pxr::UsdGeomPrimvar stPrimVar = mesh.GetPrimvar( pxr::TfToken( "st" ) );
        pxr::VtValue stValue;
        stPrimVar.Get( &stValue, time );
		std::cerr << "Got VtValue for st but not yet read "  << getCurrentRSS() << "\n";

		auto stArray = stValue.Get<pxr::VtArray<pxr::GfVec2f>>();

		const Imath::V2f *stArrayTyped = reinterpret_cast<const Imath::V2f *>( stArray.cdata() );
		Imath::V2f accumSt( 0 );
		for( unsigned int i = 0; i < stArray.size(); i++ )
		{
			accumSt += stArrayTyped[i];
		}
		std::cerr << "ACCUM st : " << accumSt << "\n";
		std::cerr << "Memory after accumulating st "  << getCurrentRSS() << "\n";
    }
	std::cerr << "Memory after releasing st "  << getCurrentRSS() << "\n";

	return MeshPrimitive::createPlane( Imath::Box2f( Imath::V2f( -1 ), Imath::V2f( 1 ) ) );

}

bool meshMightBeTimeVarying( pxr::UsdGeomMesh &mesh )
{
	return
		mesh.GetSubdivisionSchemeAttr().ValueMightBeTimeVarying() ||
		mesh.GetFaceVertexCountsAttr().ValueMightBeTimeVarying() ||
		mesh.GetFaceVertexIndicesAttr().ValueMightBeTimeVarying() ||
		mesh.GetCornerIndicesAttr().ValueMightBeTimeVarying() ||
		mesh.GetCornerSharpnessesAttr().ValueMightBeTimeVarying() ||
		mesh.GetCreaseLengthsAttr().ValueMightBeTimeVarying() ||
		mesh.GetCreaseIndicesAttr().ValueMightBeTimeVarying() ||
		mesh.GetCreaseSharpnessesAttr().ValueMightBeTimeVarying() ||
		PrimitiveAlgo::primitiveVariablesMightBeTimeVarying( mesh )
	;
}

ObjectAlgo::ReaderDescription<pxr::UsdGeomMesh> g_meshReaderDescription( pxr::TfToken( "Mesh" ), readMesh, meshMightBeTimeVarying );

} // namespace

//////////////////////////////////////////////////////////////////////////
// Writing
//////////////////////////////////////////////////////////////////////////

namespace
{

bool writeMesh( const IECoreScene::MeshPrimitive *mesh, const pxr::UsdStagePtr &stage, const pxr::SdfPath &path, pxr::UsdTimeCode time )
{
	auto usdMesh = pxr::UsdGeomMesh::Define( stage, path );

	// Topology

	usdMesh.CreateFaceVertexCountsAttr().Set( DataAlgo::toUSD( mesh->verticesPerFace() ), time );
	usdMesh.CreateFaceVertexIndicesAttr().Set( DataAlgo::toUSD( mesh->vertexIds() ), time );

	// Interpolation

	if( mesh->interpolation() == std::string( "catmullClark" ) )
	{
		usdMesh.CreateSubdivisionSchemeAttr().Set( pxr::UsdGeomTokens->catmullClark );
	}
	else
	{
		usdMesh.CreateSubdivisionSchemeAttr().Set( pxr::UsdGeomTokens->none );
	}

	// Corners

	if( mesh->cornerIds()->readable().size() )
	{
		usdMesh.CreateCornerIndicesAttr().Set( DataAlgo::toUSD( mesh->cornerIds() ), time );
		usdMesh.CreateCornerSharpnessesAttr().Set( DataAlgo::toUSD( mesh->cornerSharpnesses() ), time );
	}

	// Creases

	if( mesh->creaseLengths()->readable().size() )
	{
		usdMesh.CreateCreaseLengthsAttr().Set( DataAlgo::toUSD( mesh->creaseLengths() ), time );
		usdMesh.CreateCreaseIndicesAttr().Set( DataAlgo::toUSD( mesh->creaseIds() ), time );
		usdMesh.CreateCreaseSharpnessesAttr().Set( DataAlgo::toUSD( mesh->creaseSharpnesses() ), time );
	}

	// Primvars

	for( const auto &p : mesh->variables )
	{
		PrimitiveAlgo::writePrimitiveVariable( p.first, p.second, usdMesh, time );
	}

	return true;
}

ObjectAlgo::WriterDescription<MeshPrimitive> g_meshWriterDescription( writeMesh );

} // namespace
