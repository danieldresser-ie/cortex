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

#ifndef IECORESCENE_CAMERA_H
#define IECORESCENE_CAMERA_H

#include "IECoreScene/Export.h"
#include "IECoreScene/PreWorldRenderable.h"

namespace IECoreScene
{

class IECORESCENE_API Camera : public PreWorldRenderable
{
	public:
		enum FilmFit
		{
			Horizontal,
			Vertical,
			Fit,
			Fill,
			Distort,
		};

		Camera( IECore::CompoundDataPtr parameters = new IECore::CompoundData );
		~Camera() override;

		IE_CORE_DECLAREEXTENSIONOBJECT( Camera, CameraTypeId, PreWorldRenderable );

		IECore::CompoundDataMap &parameters();
		const IECore::CompoundDataMap &parameters() const;

		/// This is mostly of use for the binding - the parameters()
		/// function gives more direct access to the contents of the CompoundData
		/// (it calls readable() or writable() for you).
		IECore::CompoundData *parametersData();
		const IECore::CompoundData *parametersData() const;

		/// Camera parameters
		/// ------------------------------
		///
		/// These are the fundamental parameters of the camera.
		/// They are stored in the parameters of key/value pairs, but you can always
		/// just use these accessors.  get* behaves as if parameters that have not
		/// been set yet had been stored with a default value.

		std::string getProjection() const;
		void setProjection( const std::string &projection );

		Imath::V2f getAperture() const;
		void setAperture( const Imath::V2f &aperture );

		Imath::V2f getApertureOffset() const;
		void setApertureOffset( const Imath::V2f &apertureOffset );

		float getFocalLength() const;
		void setFocalLength( const float &focalLength );

		Imath::V2f getClippingPlanes() const;
		void setClippingPlanes( const Imath::V2f &clippingPlanes );

		float getFStop() const;
		void setFStop( const float &fStop );

		float getFocalLengthWorldScale() const;
		void setFocalLengthWorldScale( const float &focalLengthWorldScale );

		float getFocusDistance() const;
		void setFocusDistance( const float &focusDistance );


		/// Rendering parameters
		/// ------------------------------
		///
		/// These specify additional optional overrides of rendering settings.
		/// Usually these setting should be controlled from the render globals,
		/// but we include the option of overriding them on the camera so that :
		/// - A user can set up a projection camera, where it is important that
		///   the aspect ratio not vary with the render globals
		/// - A user can override overscan or crop on just one camera in a
		///   multi-camera setup
		/// - So that the `Renderer::camera()` method receives everything
		///   related to a particular camera in a single call, simplifying IPR edits.
		///   This means that settings from the render globals must be baked into
		///   the camera before passing to Renderer.

		bool hasFilmFit() const;
		FilmFit getFilmFit() const;
		void setFilmFit( const FilmFit &filmFit );
		void removeFilmFit();

		bool hasResolution() const;
		Imath::V2i getResolution() const;
		void setResolution( const Imath::V2i &resolution );
		void removeResolution();

		bool hasPixelAspectRatio() const;
		float getPixelAspectRatio() const;
		void setPixelAspectRatio( const float &pixelAspectRatio );
		void removePixelAspectRatio();

		bool hasResolutionMultiplier() const;
		float getResolutionMultiplier() const;
		void setResolutionMultiplier( const float &resolutionMultiplier );
		void removeResolutionMultiplier();

		bool hasOverscan() const;
		bool getOverscan() const;
		void setOverscan( const bool &overscan );
		void removeOverscan();

		bool hasOverscanLeft() const;
		float getOverscanLeft() const;
		void setOverscanLeft( const float &overscanLeft );
		void removeOverscanLeft();

		bool hasOverscanRight() const;
		float getOverscanRight() const;
		void setOverscanRight( const float &overscanRight );
		void removeOverscanRight();

		bool hasOverscanTop() const;
		float getOverscanTop() const;
		void setOverscanTop( const float &overscanTop );
		void removeOverscanTop();

		bool hasOverscanBottom() const;
		float getOverscanBottom() const;
		void setOverscanBottom( const float &overscanBottom );
		void removeOverscanBottom();

		bool hasCropWindow() const;
		Imath::Box2f getCropWindow() const;
		void setCropWindow( const Imath::Box2f &overscanBottom );
		void removeCropWindow();

		bool hasShutter() const;
		Imath::V2f getShutter() const;
		void setShutter( const Imath::V2f &shutter );
		void removeShutter();


		static Imath::Box2f fitWindow( const Imath::Box2f &window, Camera::FilmFit fitMode, float targetAspect );

		Imath::Box2f frustum() const;
		Imath::Box2f frustum( FilmFit fitMode ) const;
		Imath::Box2f frustum( FilmFit fitMode, float aspectRatio ) const;

		Imath::V2i renderResolution() const;
		Imath::Box2i renderRegion() const;

		Imath::V2f calculateFieldOfView() const;
		void setFocalLengthFromFieldOfView( float horizontalFOV );

		void render( Renderer *renderer ) const override;


	private:

		Imath::Box2f defaultApertureRect() const;

		IECore::CompoundDataPtr m_parameters;

		static const unsigned int m_ioVersion;
};

IE_CORE_DECLAREPTR( Camera );

}

#endif // IECORESCENE_CAMERA_H
