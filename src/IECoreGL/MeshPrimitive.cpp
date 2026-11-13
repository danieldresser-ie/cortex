//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2007-2010, Image Engine Design Inc. All rights reserved.
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

#include "IECoreGL/MeshPrimitive.h"

#include "IECoreGL/CachedConverter.h"
#include "IECoreGL/GL.h"
#include "IECoreGL/State.h"
#include "IECoreGL/ToGLConverter.h"

#include "IECore/DataAlgo.h"
#include "IECore/DespatchTypedData.h"

#include "Imath/ImathMath.h"

#include <cassert>

using namespace IECoreGL;
using namespace Imath;
using namespace std;

namespace
{

void triangulatedMeshIndices(
	const IECoreScene::MeshPrimitive *mesh,
	IECoreScene::PrimitiveVariable::Interpolation varInterpolation,
	std::vector<int> &newIds,
	const IECore::Canceller *canceller
)
{
	const std::vector<int> &verticesPerFace = mesh->verticesPerFace()->readable();

	newIds.clear();

	int numTris = 0;
	for( auto n : verticesPerFace )
	{
		numTris += n - 2;
	}

	int faceVertexIdStart = 0;
	for( int faceIdx = 0; faceIdx < (int)verticesPerFace.size(); faceIdx++ )
	{
		if( ( faceIdx % 100 ) == 0 )
		{
			IECore::Canceller::check( canceller );
		}

		int numFaceVerts = verticesPerFace[ faceIdx ];

		const int i0 = faceVertexIdStart + 0;

		for( int i = 1; i < numFaceVerts - 1; i++ )
		{
			const int i1 = faceVertexIdStart + i;
			const int i2 = faceVertexIdStart + i + 1;

			/// Store the indices required to rebuild the facevarying primvars
			newIds.push_back( i0 );
			newIds.push_back( i1 );
			newIds.push_back( i2 );
		}

		faceVertexIdStart += numFaceVerts;
	}

	if( varInterpolation == IECoreScene::PrimitiveVariable::Vertex || varInterpolation == IECoreScene::PrimitiveVariable::Varying )
	{
		const std::vector<int> &vertexIds = mesh->vertexIds()->readable();
		for( int &i : newIds )
		{
			i = vertexIds[i];
		}
	}
}

void wireframeMeshIndices(
	const IECoreScene::MeshPrimitive *mesh,
	IECoreScene::PrimitiveVariable::Interpolation varInterpolation,
	std::vector<int> &newIds,
	const IECore::Canceller *canceller
)
{
	const std::vector<int> &verticesPerFace = mesh->verticesPerFace()->readable();
	const std::vector<int> &vertexIds = mesh->vertexIds()->readable();

	std::vector< std::pair<int,int> > edges;


	int maxNumEdges = 0;
	for( auto n : verticesPerFace )
	{
		maxNumEdges += n;
	}

	edges.reserve( maxNumEdges );

	int faceVertexIdStart = 0;
	for( int faceIdx = 0; faceIdx < (int)verticesPerFace.size(); faceIdx++ )
	{
		if( ( faceIdx % 100 ) == 0 )
		{
			IECore::Canceller::check( canceller );
		}

		int numFaceVerts = verticesPerFace[ faceIdx ];

		int prevVertId = vertexIds[faceVertexIdStart + numFaceVerts - 1];

		for( int i = 0; i < numFaceVerts; i++ )
		{
			const int vertId = vertexIds[faceVertexIdStart + i];

			edges.push_back( std::make_pair(
				std::min( vertId, prevVertId ),
				std::max( vertId, prevVertId )
			) );

			prevVertId = vertId;
		}

		faceVertexIdStart += numFaceVerts;
	}

	// In common meshes, most edges have now been added twice. Rather than using a map to collect them,
	// we can just sort and uniquify now.

	std::sort( edges.begin(), edges.end() );
	edges.erase(
		std::unique( edges.begin(), edges.end() ),
		edges.end()
	);

	newIds.clear();
	newIds.reserve( edges.size() * 2 );
	for( const auto &i : edges )
	{
		newIds.push_back( i.first );
		newIds.push_back( i.second );
	}


	if( varInterpolation == IECoreScene::PrimitiveVariable::FaceVarying )
	{
		std::vector<int> reverseMapping( mesh->variableSize( IECoreScene::PrimitiveVariable::Vertex ), 0 );
		for( int i = vertexIds.size() - 1; i >= 0; i-- )
		{
			reverseMapping[ vertexIds[i] ] = i;
		}
		for( int &i : newIds )
		{
			i = reverseMapping[i];
		}
	}
}

enum class MeshTopologyPurpose
{
	TriangulatedMeshIndices,
	WireframeIndices
};

class MeshTopologyInputWrapper : public IECore::Object
{
public:

	IE_CORE_DECLAREEXTENSIONOBJECT( MeshTopologyInputWrapper, MeshTopologyInputWrapperTypeId, IECore::Object );

	MeshTopologyInputWrapper()
	{
		throw IECore::Exception( "Must be constructed explicitly" );
	}

	MeshTopologyInputWrapper(
		IECoreScene::ConstMeshPrimitivePtr sourceMesh,
		IECoreScene::PrimitiveVariable::Interpolation interpolation,
		MeshTopologyPurpose purpose
	) : sourceMesh( sourceMesh ), interpolation( interpolation ), purpose( purpose )
	{
	}


	IECoreScene::ConstMeshPrimitivePtr sourceMesh;
	IECoreScene::PrimitiveVariable::Interpolation interpolation;
	MeshTopologyPurpose purpose;
};

IE_CORE_DEFINEOBJECTTYPEDESCRIPTION( MeshTopologyInputWrapper );
IE_CORE_DECLAREPTR( MeshTopologyInputWrapper );

void MeshTopologyInputWrapper::copyFrom( const Object *other, CopyContext *context )
{
	Object::copyFrom( other, context );
	const MeshTopologyInputWrapper *tOther = static_cast<const MeshTopologyInputWrapper *>( other );
	sourceMesh = tOther->sourceMesh;
	interpolation = tOther->interpolation;
	purpose = tOther->purpose;
}

void MeshTopologyInputWrapper::save( SaveContext *context ) const
{
	throw IECore::Exception( "NOT IMPLEMENTED" );
}

void MeshTopologyInputWrapper::load( LoadContextPtr context )
{
	throw IECore::Exception( "NOT IMPLEMENTED" );
}

bool MeshTopologyInputWrapper::isEqualTo( const Object *other ) const
{
	if( !Object::isEqualTo( other ) )
	{
		return false;
	}
	const MeshTopologyInputWrapper *tOther = static_cast<const MeshTopologyInputWrapper *>( other );
	return *sourceMesh == *tOther->sourceMesh &&
		interpolation == tOther->interpolation &&
		purpose == tOther->purpose;
}

void MeshTopologyInputWrapper::hash( IECore::MurmurHash &h ) const
{
	sourceMesh->topologyHash( h );
	h.append( interpolation );
	h.append( purpose );
}

void MeshTopologyInputWrapper::memoryUsage( IECore::Object::MemoryAccumulator &accumulator ) const
{
	accumulator.accumulate( sourceMesh.get() );
}

//IE_CORE_DECLAREPTR( MeshTopologyInputWrapper );

/// Converts IECore::MeshPrimitive objects into IECoreGL::MeshPrimitive objects.
/// \ingroup conversionGroup
class ToGLMeshTopologyConverter : public ToGLConverter
{

	public :

		typedef MeshTopologyInputWrapper InputType;
		typedef IECoreGL::Buffer ResultType;

		IE_CORE_DECLARERUNTIMETYPEDEXTENSION( ToGLMeshTopologyConverter, ToGLMeshTopologyConverterTypeId, ToGLConverter );
		ToGLMeshTopologyConverter( ConstMeshTopologyInputWrapperPtr toConvert = nullptr )
			:   ToGLConverter(
				"Converts internal mesh topology obects to GL representations, used when converting "
				"IECoreScene::MeshPrimitive objects to IECoreGL::MeshPrimitive objects.",
				MeshTopologyInputWrapper::staticTypeId()
			)
		{
			srcParameter()->setValue( boost::const_pointer_cast<MeshTopologyInputWrapper>( toConvert ) );
		}

		~ToGLMeshTopologyConverter() override
		{
		}

	protected :

		IECore::RunTimeTypedPtr doConversion( IECore::ConstObjectPtr src, IECore::ConstCompoundObjectPtr operands ) const override
		{
			// safe because the parameter validated it for us
			ConstMeshTopologyInputWrapperPtr input = boost::static_pointer_cast<const MeshTopologyInputWrapper>( src );

			const IECoreScene::MeshPrimitive* mesh = input->sourceMesh.get();
			if( !mesh )
			{
				throw IECore::Exception( "TODO" );
			}

			IECore::IntVectorDataPtr resultIndices = new IECore::IntVectorData();
			if( input->purpose == MeshTopologyPurpose::WireframeIndices )
			{
				wireframeMeshIndices( mesh, input->interpolation, resultIndices->writable(), nullptr );
			}
			else
			{
				triangulatedMeshIndices( mesh, input->interpolation, resultIndices->writable(), nullptr );
			}

			//return IECoreGL::CachedConverter::defaultCachedConverter()->convert( resultIndices.get() );
			return new IECoreGL::Buffer( address( resultIndices.get() ), sizeof( int ) * resultIndices->readable().size() );
		}

	private :

		static ConverterDescription<ToGLMeshTopologyConverter> g_description;

};

IE_CORE_DECLAREPTR( ToGLMeshTopologyConverter );

IE_CORE_DEFINERUNTIMETYPED( ToGLMeshTopologyConverter );

ToGLConverter::ConverterDescription<ToGLMeshTopologyConverter> ToGLMeshTopologyConverter::g_description;


}

//////////////////////////////////////////////////////////////////////////
// MemberData
//////////////////////////////////////////////////////////////////////////

class MeshPrimitive::MemberData : public IECore::RefCounted
{

	public :

		MemberData( IECoreScene::ConstMeshPrimitivePtr mesh )
			: mesh( mesh )
		{
		}

		IECoreScene::ConstMeshPrimitivePtr mesh;
		IECoreScene::PrimitiveVariable::Interpolation targetInterpolation;
		IECoreGL::ConstBufferPtr triangulatedMeshIndices;
		IECoreGL::ConstBufferPtr wireframeIndices;
		Imath::Box3f bound;

};

//////////////////////////////////////////////////////////////////////////
// MeshPrimitive
//////////////////////////////////////////////////////////////////////////

IE_CORE_DEFINERUNTIMETYPED( MeshPrimitive );


MeshPrimitive::MeshPrimitive( unsigned numTriangles )
{
	throw IECore::Exception( "Old constructor disabled for testing purposes" );
}


MeshPrimitive::MeshPrimitive( IECoreScene::ConstMeshPrimitivePtr mesh )
	:	m_memberData( new MemberData( mesh ) )
{
	IECoreScene::PrimitiveVariable::Interpolation targetInterpolation = IECoreScene::PrimitiveVariable::Vertex;
	for( auto i : m_memberData->mesh->variables )
	{
		if( !(
			i.second.interpolation == IECoreScene::PrimitiveVariable::Vertex ||
			i.second.interpolation == IECoreScene::PrimitiveVariable::Varying ||
			i.second.interpolation == IECoreScene::PrimitiveVariable::Constant
		) )
		{
			targetInterpolation = IECoreScene::PrimitiveVariable::FaceVarying;
		}
	}

	m_memberData->targetInterpolation = targetInterpolation;
}

MeshPrimitive::~MeshPrimitive()
{
}

void MeshPrimitive::addPrimitiveVariable( const std::string &name, const IECoreScene::PrimitiveVariable &primVar )
{
	if( name == "P" )
	{
		// update the bounding box.
		m_memberData->bound.makeEmpty();
		IECore::ConstV3fVectorDataPtr points = IECore::runTimeCast< IECore::V3fVectorData >( primVar.data );
		if( points )
		{
			const std::vector<Imath::V3f> &p = points->readable();
			for( unsigned int i=0; i<p.size(); i++ )
			{
				m_memberData->bound.extendBy( p[i] );
			}
		}
	}

	if ( primVar.interpolation==IECoreScene::PrimitiveVariable::FaceVarying || primVar.interpolation==IECoreScene::PrimitiveVariable::Vertex || primVar.interpolation==IECoreScene::PrimitiveVariable::Varying )
	{
		addVertexAttribute( name, primVar.expandedData() );
	}
	else if ( primVar.interpolation==IECoreScene::PrimitiveVariable::Constant )
	{
		addUniformAttribute( name, primVar.expandedData() );
	}
}

void MeshPrimitive::render( const State *currentState, IECore::TypeId style ) const
{

	if( style == Primitive::DrawWireframe::staticTypeId() )
	{
		if( !m_memberData->wireframeIndices )
		{
			MeshTopologyInputWrapperPtr topologyInputWrapper = new MeshTopologyInputWrapper(
				m_memberData->mesh, m_memberData->targetInterpolation, MeshTopologyPurpose::WireframeIndices
			);

			m_memberData->wireframeIndices = IECore::runTimeCast< const IECoreGL::Buffer >(
				CachedConverter::defaultCachedConverter()->convert( topologyInputWrapper.get() )
			);
		}

		IECoreGL::Buffer::ScopedBinding binding( *m_memberData->wireframeIndices, GL_ELEMENT_ARRAY_BUFFER );
		glDrawElements( GL_LINES, m_memberData->wireframeIndices->size() / sizeof( int ), GL_UNSIGNED_INT, 0 );
	}
	else
	{
		renderInstances( 1 );
	}
}

void MeshPrimitive::renderInstances( size_t numInstances ) const
{
	if( !m_memberData->triangulatedMeshIndices )
	{
		MeshTopologyInputWrapperPtr topologyInputWrapper = new MeshTopologyInputWrapper(
			m_memberData->mesh, m_memberData->targetInterpolation, MeshTopologyPurpose::TriangulatedMeshIndices
		);

		m_memberData->triangulatedMeshIndices = IECore::runTimeCast< const IECoreGL::Buffer >(
			CachedConverter::defaultCachedConverter()->convert( topologyInputWrapper.get() )
		);
	}

	IECoreGL::Buffer::ScopedBinding binding( *m_memberData->triangulatedMeshIndices, GL_ELEMENT_ARRAY_BUFFER );
	glDrawElementsInstancedARB(
		GL_TRIANGLES, m_memberData->triangulatedMeshIndices->size() / sizeof( int ), GL_UNSIGNED_INT, 0,
		numInstances
	);
}

Imath::Box3f MeshPrimitive::bound() const
{
	return m_memberData->bound;
}
