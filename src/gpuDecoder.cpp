#include <gpuDecoder.h>

void GpuDecoder::recreateBuffer(size_t number)
{
    TextureBuffer *currentBuffer = &buffers[bufferIndex]; 
    ge::gl::glVDPAUUnmapSurfacesNV (currentBuffer->nvSurfaces.size(), currentBuffer->nvSurfaces.data());
    for(auto surface : currentBuffer->nvSurfaces)
        ge::gl::glVDPAUUnregisterSurfaceNV(surface);
    currentBuffer->nvSurfaces.clear();
    currentBuffer->nvSurfaces.resize(number);
    for(auto surface : currentBuffer->vdpSurfaces)
        vdp_output_surface_destroy(surface);
    currentBuffer->vdpSurfaces.clear();
    currentBuffer->vdpSurfaces.resize(number);
    currentBuffer->textureHandles.clear();
    currentBuffer->textureHandles.resize(number);
    ge::gl::glDeleteTextures(currentBuffer->textures.size(), currentBuffer->textures.data());
    currentBuffer->textures.clear();
    currentBuffer->textures.resize(number);
    ge::gl::glCreateTextures(GL_TEXTURE_2D, number, currentBuffer->textures.data());
    for(auto &surface : currentBuffer->vdpSurfaces)
        if(vdp_output_surface_create(vdpauContext->device, VDP_RGBA_FORMAT_B8G8R8A8, codecContext->width, codecContext->height, &surface) != VDP_STATUS_OK)
            throw std::runtime_error("Cannot create VDPAU output surface.");
}

void GpuDecoder::recreateBufferLite(size_t number)
{
    TextureBuffer *currentBuffer = &buffers[bufferIndex]; 
    //ge::gl::glVDPAUUnmapSurfacesNV (currentBuffer->nvSurfaces.size(), currentBuffer->nvSurfaces.data());
    for(auto surface : currentBuffer->nvSurfaces)
        ge::gl::glVDPAUUnregisterSurfaceNV(surface);
    /*currentBuffer->nvSurfaces.clear();
      currentBuffer->nvSurfaces.resize(number);*/
    /*for(auto surface : currentBuffer->vdpSurfaces)
      vdp_output_surface_destroy(surface);*/
    /*    currentBuffer->vdpSurfaces.clear();
          currentBuffer->vdpSurfaces.resize(number);*/
    //    currentBuffer->textureHandles.clear();
    //    currentBuffer->textureHandles.resize(number);
    //  ge::gl::glDeleteTextures(currentBuffer->textures.size(), currentBuffer->textures.data());
    //currentBuffer->textures.clear();
    //currentBuffer->textures.resize(number);
    ge::gl::glCreateTextures(GL_TEXTURE_2D, number, currentBuffer->textures.data());
}

void GpuDecoder::seek(int frameNum)
{
    AVStream *stream = formatContext->streams[videoStreamId];
    //uint64_t time = stream->time_base.den * stream->r_frame_rate.den * frameNum / (stream->time_base.num * stream->r_frame_rate.num);
    if(av_seek_frame(formatContext, videoStreamId, frameNum, AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME) < 0)
        throw std::runtime_error("Cannot seek"); 
    avcodec_flush_buffers(codecContext);
    av_read_frame(formatContext, &packet);
}

//convert as -c:v libx265 -pix_fmt yuv420p (pix fmt, only 420 avaliable for gpu dec)
std::vector<uint64_t> GpuDecoder::getFrames(size_t number)
{ 
    swapBuffers();
    //the clearing can be avoided if we specify a fixed amount for getframes when constructing
    //recreateBuffer(number);
    if(number != lastFrameCount)
    {
        recreateBuffer(number);
        swapBuffers();
        recreateBuffer(number);
        swapBuffers();
        lastFrameCount = number;
    }
    else
        recreateBufferLite(number);

    TextureBuffer *currentBuffer = getCurrentBuffer(); 

    for(int i=0; i<number; i++)
    {
        bool send = true;
        while(send)
        {
            if(packet.stream_index != videoStreamId)
            {
                av_packet_unref(&packet);
                continue;
            }
            else if(avcodec_send_packet(codecContext, &packet) != 0)
                break;
            av_packet_unref(&packet);
            send = (av_read_frame(formatContext, &packet) == 0); 
        }

        bool waitForFrame = true;
        while(waitForFrame)
        {
            AVFrame *frame = av_frame_alloc();
            if(!frame)
                throw std::runtime_error("Cannot allocate packet/frame");

            int err = avcodec_receive_frame(codecContext, frame);
            if(err == AVERROR(EAGAIN))
            {
                av_packet_unref(&packet);
                avcodec_send_packet(codecContext, &packet);
            }
            else if(err == AVERROR_EOF)
            {
                av_frame_free(&frame);
                //TODO reset when asking for more
                /*  auto stream = formatContext->streams[videoStreamId];
                    avio_seek(formatContext->pb, 0, SEEK_SET);
                //avformat_seek_file(formatContext, videoStreamId, 0, 0, stream->duration, 0);*/
                //                avcodec_flush_buffers(codecContext);
                /*av_seek_frame(formatContext, videoStreamId, 0, 0);
                avcodec_flush_buffers(codecContext);
                av_read_frame(formatContext, &packet);*/
                seek(0);
                std::cerr<<"END";
                //waitForFrame = false;
                break;
            }
            else if(err < 0)
                throw std::runtime_error("Cannot receive frame");

            if(frame->format == pixFmt)
            {
                if(vdp_video_mixer_render(mixer, VDP_INVALID_HANDLE, nullptr, VDP_VIDEO_MIXER_PICTURE_STRUCTURE_FRAME, 0, nullptr, (VdpVideoSurface)(uintptr_t)frame->data[3], 0, nullptr, &flipRect, currentBuffer->vdpSurfaces[i], nullptr, nullptr, 0, nullptr) != VDP_STATUS_OK)
                    throw std::runtime_error("VDP mixer error!");

                currentBuffer->nvSurfaces[i] = ge::gl::glVDPAURegisterOutputSurfaceNV((void *)(uintptr_t)currentBuffer->vdpSurfaces[i],GL_TEXTURE_2D,1,&currentBuffer->textures[i]);

                ge::gl::glVDPAUSurfaceAccessNV(currentBuffer->nvSurfaces[i], GL_READ_ONLY);
                ge::gl::glVDPAUMapSurfacesNV (1, &currentBuffer->nvSurfaces[i]);

                currentBuffer->textureHandles[i] = ge::gl::glGetTextureHandleARB(currentBuffer->textures[i]);
                //ge::gl::glMakeTextureHandleResidentARB(currentBuffer->textureHandles[i]);

                waitForFrame = false;
                //std::cerr << i << " ";
            } 
            av_frame_free(&frame);
        }    
    }
    // std::cerr << std::endl;
    return currentBuffer->textureHandles; 
}

GpuDecoder::GpuDecoder(const char* path)
{
    formatContext = avformat_alloc_context();
    if(!formatContext)
        throw std::runtime_error("Cannot allocate format context memory");

    if(avformat_open_input(&formatContext, path, NULL, NULL) != 0)
        throw std::runtime_error("Cannot open the video file"); 

    if(avformat_find_stream_info(formatContext, NULL) < 0)
        throw std::runtime_error("Cannot get the stream info");

    videoStreamId = av_find_best_stream(formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
    if(videoStreamId < 0)
        throw std::runtime_error("No video stream available");

    if(!codec)
        throw std::runtime_error("No suitable codec found");

    codecContext = avcodec_alloc_context3(codec);
    if(!codecContext)
        throw std::runtime_error("Cannot allocate codec context memory");

    if(avcodec_parameters_to_context(codecContext, formatContext->streams[videoStreamId]->codecpar)<0)
        throw std::runtime_error{"Cannot use the file parameters in context"};

    const AVCodecHWConfig *config;
    for(int i=0;; i++)
    {
        if(!(config = avcodec_get_hw_config(codec, i)))
            throw std::runtime_error("No HW config for codec");
        //vaapi libva-vdpau-driver for nvidia needed
        else if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && config->device_type == AV_HWDEVICE_TYPE_VDPAU)
        {   
            pixFmt = config->pix_fmt;
            //std::cerr << av_get_pix_fmt_name(pixFmt) << std::endl;
            break;
        }
        //std::cerr << av_hwdevice_get_type_name(config->device_type) << std::endl;
    }

    if(av_hwdevice_ctx_create(&deviceContext, config->device_type, NULL, NULL, 0) < 0)
        throw std::runtime_error("Cannot create HW device");
    codecContext->hw_device_ctx = av_buffer_ref(deviceContext);

    if(avcodec_open2(codecContext, codec, NULL) < 0)
        throw std::runtime_error("Cannot open codec.");

    /*
       AVBufferRef *avClContext = nullptr;
       std::vector<char> buff(100);
       int e;
       if(e = av_hwdevice_ctx_create_derived(&avClContext, AV_HWDEVICE_TYPE_OPENCL, deviceContext, 0) < 0)
       {
       av_make_error_string(buff.data(), buff.size(),e);
       std::cerr<<buff.data() << std::endl;
       throw std::runtime_error("Cannot create derived CL context");
       }*/
    AVHWDeviceContext *device_ctx = (AVHWDeviceContext*)deviceContext->data;
    vdpauContext = reinterpret_cast<AVVDPAUDeviceContext*>(device_ctx->hwctx);
    ge::gl::glVDPAUInitNV((void*)(uintptr_t)(vdpauContext->device), (void*)vdpauContext->get_proc_address);

    VdpGetProcAddress *get_proc_address = vdpauContext->get_proc_address;
    /*    VdpVideoSurfaceGetParameters *vdp_video_surface_get_parameters;
          get_proc_address(k->device, VDP_FUNC_ID_VIDEO_SURFACE_GET_PARAMETERS, (void**)&vdp_video_surface_get_parameters);*/
    get_proc_address(vdpauContext->device, VDP_FUNC_ID_OUTPUT_SURFACE_CREATE, (void**)&vdp_output_surface_create);
    get_proc_address(vdpauContext->device, VDP_FUNC_ID_OUTPUT_SURFACE_DESTROY, (void**)&vdp_output_surface_destroy);
    get_proc_address(vdpauContext->device, VDP_FUNC_ID_VIDEO_MIXER_CREATE, (void**)&vdp_video_mixer_create);
    get_proc_address(vdpauContext->device, VDP_FUNC_ID_VIDEO_MIXER_RENDER, (void**)&vdp_video_mixer_render);

    uint32_t w = codecContext->width;
    uint32_t h = codecContext->height;
    flipRect = {0,h,w,0};
    /*    vars.addUint32("lf.width", w);
          vars.addUint32("lf.height", h);
          vars.addFloat("texture.aspect",(float)w/(float)h);*/
    VdpChromaType ct = VDP_CHROMA_TYPE_420;
    VdpVideoMixerParameter params[] = {
        VDP_VIDEO_MIXER_PARAMETER_CHROMA_TYPE,
        VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_WIDTH,
        VDP_VIDEO_MIXER_PARAMETER_VIDEO_SURFACE_HEIGHT};
    void *param_vals[] = {&ct,&w,&h};
    /*if(vdp_video_surface_get_parameters((VdpVideoSurface)(uintptr_t)frame->data[3], &ct, &w, &h) != VDP_STATUS_OK)
      throw std::runtime_error("Cannot get surface parameters.");*/

    if(vdp_video_mixer_create(vdpauContext->device, 0, nullptr, 3, params, param_vals, &mixer) != VDP_STATUS_OK)
        throw std::runtime_error("Cannot create VDPAU mixer.");

    //start reading
    av_read_frame(formatContext, &packet);
}

