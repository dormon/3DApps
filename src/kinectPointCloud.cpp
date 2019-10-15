#include <kinectPointCloud.h>
#include <geGL/StaticCalls.h>
#include <geGL/geGL.h>
#include <BasicCamera/Camera.h>
#include <BasicCamera/PerspectiveCamera.h>
#include <Barrier.h>
#include <imguiSDL2OpenGL/imgui.h>
#include <addVarsLimits.h>
#include <azureKinectUtils.h>
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/registration/icp.h>
#include <pcl/point_representation.h>

#include <pcl/filters/voxel_grid.h>
#include <pcl/filters/filter.h>
#include <pcl/features/normal_3d.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>
#include <pcl/io/ply_io.h>
#include <pcl/filters/filter.h>


#include <opencv2/objdetect.hpp> 
#include <opencv2/imgproc.hpp> 
#include <opencv2/highgui.hpp>

int colorModeIndex{0};
int depthModeIndex{0};
const k4a_color_resolution_t colorModes[]{K4A_COLOR_RESOLUTION_720P, K4A_COLOR_RESOLUTION_2160P, K4A_COLOR_RESOLUTION_1440P, K4A_COLOR_RESOLUTION_1080P, K4A_COLOR_RESOLUTION_3072P, K4A_COLOR_RESOLUTION_1536P};
const k4a_depth_mode_t depthModes[]{K4A_DEPTH_MODE_NFOV_UNBINNED, K4A_DEPTH_MODE_WFOV_UNBINNED, K4A_DEPTH_MODE_NFOV_2X2BINNED, K4A_DEPTH_MODE_WFOV_2X2BINNED};
constexpr int DEVICE_COUNT{2};
glm::mat4 camMatrices[DEVICE_COUNT]; 

template<typename T, int m, int n>
inline glm::mat<m, n, float, glm::precision::highp> E2GLM(const Eigen::Matrix<T, m, n>& em)
{
    glm::mat<m, n, float, glm::precision::highp> mat;
    for (int i = 0; i < m; ++i)
    {
        for (int j = 0; j < n; ++j)
        {
            mat[j][i] = em(i, j);
        }
    }
    return mat;
}
inline Eigen::Matrix4f GLM2E(glm::mat4& mat)
{
    Eigen::Matrix4f em;
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            em(j,i) = mat[i][j];
        }
    }
    return em;
}

void createKPCProgram(vars::Vars&vars){
    if(notChanged(vars,"all",__FUNCTION__,{}))return;

    std::string vsSrc = R".(
  uniform mat4 projView;
  out vec3 vColor;

  layout(binding=DEVICE_COUNT*2)uniform sampler2D xyTexture;
  layout(binding=0)uniform sampler2D colorTextures[DEVICE_COUNT];
  layout(binding=DEVICE_COUNT)uniform isampler2D depthTextures[DEVICE_COUNT];

  uniform vec2 clipRange;
  uniform float sampleRate;
  uniform mat4 camMatrices[DEVICE_COUNT];

  void main(){
    vec2 coord;
    vec2 size = textureSize(depthTextures[0],0)*sampleRate;
    int pixels = int(size.x*size.y);
    int index = gl_VertexID/pixels;
    int pixelIndex = gl_VertexID-index*pixels;
    coord.x = mod(pixelIndex,size.x)/size.x;
    coord.y = (pixelIndex/size.x)/size.y;
    coord = clamp(coord,0.0,1.0);
    float depth = float(texture(depthTextures[index],coord).x);
    /*depth += float(texture(depthTextures[index],coord+0.001).x);
    depth += float(texture(depthTextures[index],coord-0.001).x);
    depth += float(texture(depthTextures[index],coord+vec2(0.001,-0.001)).x);
    depth += float(texture(depthTextures[index],coord+vec2(-0.001,0.001)).x);
    depth /=4.0;*/
    //float depth = float(texelFetch(depthTextures[index],coord,0).x);
    vec2 xy = texture(xyTexture, coord, 0).xy;
    vec3 pos = vec3(depth*xy.x, depth*xy.y, depth); 
    if(depth == 0 || depth < clipRange.s || depth > clipRange.t || (xy.x == 0.0 && xy.y == 0.0)) 
    {
        gl_Position = vec4(2,2,2,1);
        return;
    }

    pos *= 0.001f;
    pos *= -1;
    vColor = texture(colorTextures[index],coord).xyz;
    if(length(vColor) < 0.1) 
    {
        gl_Position = vec4(2,2,2,1);
        return;
    }

    gl_Position = projView * camMatrices[index] * vec4(pos,1);
  }
  ).";

    std::string const fsSrc = R".(
  out vec4 fColor;
  in  vec3 vColor;
  void main(){
    fColor = vec4(vColor,1);
  }
  ).";
    
    const std::string s = "DEVICE_COUNT";
    const std::string t = std::to_string(DEVICE_COUNT);
    std::string::size_type n = 0;
    while ( ( n = vsSrc.find( s, n ) ) != std::string::npos )
    {
        vsSrc.replace( n, s.size(), t );
        n += t.size();
    }

    auto vs = std::make_shared<ge::gl::Shader>(GL_VERTEX_SHADER,
            "#version 450\n",
            vsSrc
            );
    auto fs = std::make_shared<ge::gl::Shader>(GL_FRAGMENT_SHADER,
            "#version 450\n",
            fsSrc
            );
    vars.reCreate<ge::gl::Program>("pointCloudProgram",vs,fs)->setNonexistingUniformWarning(false);
}

void createXyTable(const k4a_calibration_t *calibration, k4a::image *xyTable)
{
    k4a_float2_t *table_data = (k4a_float2_t *)(void *)xyTable->get_buffer();

    int width = calibration->depth_camera_calibration.resolution_width;
    int height = calibration->depth_camera_calibration.resolution_height;

    k4a_float2_t p;
    k4a_float3_t ray;
    int valid;

    for (int y = 0, idx = 0; y < height; y++)
    {
        p.xy.y = (float)y;
        for (int x = 0; x < width; x++, idx++)
        {
            p.xy.x = (float)x;

            k4a_calibration_2d_to_3d(
                    calibration, &p, 1.f, K4A_CALIBRATION_TYPE_DEPTH, K4A_CALIBRATION_TYPE_DEPTH, &ray, &valid);

            if (valid)
            {
                table_data[idx].xy.x = ray.xyz.x;
                table_data[idx].xy.y = ray.xyz.y;
            }
            else
            {
                table_data[idx].xy.x = nanf("");
                table_data[idx].xy.y = nanf("");
            }
        }
    }
}

void alignClouds(vars::Vars&vars)
{
    pcl::PointCloud<pcl::PointXYZ>::Ptr refCloud (new pcl::PointCloud<pcl::PointXYZ>);
    auto xyTable = vars.get<k4a::image>("xyTable");
    k4a_float2_t *xyData = reinterpret_cast<k4a_float2_t*>(xyTable->get_buffer());
    auto size = vars.get<glm::ivec2>("depthSize"); 
    for(int i=0; i<DEVICE_COUNT; i++)
    {
        k4a::capture capture;
        std::string index = std::to_string(i);
        auto dev = vars.get<k4a::device>("kinectDevice"+index);
        if (dev->get_capture(&capture, std::chrono::milliseconds(0)))
        {
            const k4a::image depthImage = capture.get_depth_image();
            const uint16_t *depthData = reinterpret_cast<const uint16_t*>(depthImage.get_buffer());
            
            pcl::PointCloud<pcl::PointXYZ>::Ptr cloud (new pcl::PointCloud<pcl::PointXYZ>);
            constexpr int D = 1;
            cloud->width = size->x/D; 
            cloud->height = size->y/D; 
            cloud->is_dense = false;
            cloud->points.resize(cloud->width*cloud->height);
            constexpr int M = D*D; 
            for (size_t j=0; j<cloud->points.size(); j++)
            {   if(depthData[j*M] > 1000)
                {    
                cloud->points[j].x = std::numeric_limits<float>::quiet_NaN();
                cloud->points[j].y = 0;
                cloud->points[j].z = 0;
                }
                else
                {
            /*    glm::vec4 p(1.0);
                p.x = xyData[j*M].xy.x * depthData[j*M]*0.001;
                p.y = xyData[j*M].xy.y * depthData[j*M]*0.001;
                p.z = depthData[j*M]*0.001;

                p = camMatrices[i]*p;

                cloud->points[j].x = p.x;
                cloud->points[j].y = p.y;
                cloud->points[j].z = p.z;*/
                cloud->points[j].x = xyData[j*M].xy.x * depthData[j*M]*-0.001;
                cloud->points[j].y = xyData[j*M].xy.y * depthData[j*M]*-0.001;
                cloud->points[j].z = depthData[j*M]*-0.001;
                }
            }

//XXXXX
class MyPointRepresentation : public pcl::PointRepresentation <pcl::PointNormal>
{
  using pcl::PointRepresentation<pcl::PointNormal>::nr_dimensions_;
public:
  MyPointRepresentation ()
  {
    // Define the number of dimensions
    nr_dimensions_ = 4;
  }

  // Override the copyToFloatArray method to define our feature vector
  virtual void copyToFloatArray (const pcl::PointNormal &p, float * out) const
  {
    // < x, y, z, curvature >
    out[0] = p.x;
    out[1] = p.y;
    out[2] = p.z;
    out[3] = p.curvature;
  }
};

     pcl::PointCloud<pcl::PointXYZ>::Ptr b (new pcl::PointCloud<pcl::PointXYZ>);
     pcl::copyPointCloud(*cloud,*b);
     pcl::PointCloud<pcl::PointXYZ>::Ptr tgt (new pcl::PointCloud<pcl::PointXYZ>);
     pcl::VoxelGrid<pcl::PointXYZ> grid;
     grid.setLeafSize (0.01, 0.01, 0.01);
     grid.setInputCloud (cloud);
     grid.filter(*tgt);

    pcl::PointCloud<pcl::PointNormal>::Ptr npc (new pcl::PointCloud<pcl::PointNormal>);
pcl::NormalEstimation<pcl::PointXYZ, pcl::PointNormal> norm_est;
  pcl::search::KdTree<pcl::PointXYZ>::Ptr tree (new pcl::search::KdTree<pcl::PointXYZ> ());
  norm_est.setSearchMethod (tree);
  norm_est.setKSearch (30);
  

  norm_est.setInputCloud (tgt);
  norm_est.compute (*npc);
  pcl::copyPointCloud (*tgt, *npc);
  cloud = tgt;
//XXXXX


const k4a::image colorImage = capture.get_color_image();
const uint8_t* buffer = colorImage.get_buffer();
int rows = colorImage.get_height_pixels();
int cols = colorImage.get_width_pixels();
cv::Mat colorMat(rows , cols, CV_8UC4, (void*)buffer, cv::Mat::AUTO_STEP);
auto trans = vars.get<k4a::transformation>("transformation");
k4a::image tImage = trans->depth_image_to_color_camera(depthImage);
if(!colorMat.empty())
cv::imwrite("c"+std::to_string(i)+".png",colorMat);
const uint8_t* bufferd = tImage.get_buffer();
cv::Mat depthMat(rows , cols, CV_16UC1, (void*)bufferd, cv::Mat::AUTO_STEP);
if(!colorMat.empty())
cv::imwrite("d"+std::to_string(i)+".exr",depthMat);


std::vector<int> indices;
pcl::removeNaNFromPointCloud(*cloud, *cloud, indices);
            if(i>0)
            {
                pcl::PointCloud<pcl::PointXYZ>::Ptr preAligned (new pcl::PointCloud<pcl::PointXYZ> ());
                pcl::transformPointCloud (*cloud, *preAligned, GLM2E(camMatrices[i]));
                pcl::IterativeClosestPoint<pcl::PointXYZ, pcl::PointXYZ> icp;
                icp.setInputSource(preAligned);
                icp.setInputTarget(refCloud);
                icp.setMaxCorrespondenceDistance (0.2);
                icp.setMaximumIterations (200);
                icp.setTransformationEpsilon (1e-12);
                icp.setEuclideanFitnessEpsilon (1e-5);
                icp.setRANSACOutlierRejectionThreshold (0.001);
                pcl::PointCloud<pcl::PointXYZ> res;
                icp.align(res);
if (icp.hasConverged())
  {
      std::cout << "ICP converged." << std::endl
                << "The score is " << icp.getFitnessScore() << std::endl;
      std::cout << "Transformation matrix:" << std::endl;
      std::cout << icp.getFinalTransformation() << std::endl;
  }
  else std::cout << "ICP did not converge." << std::endl;

                auto m = icp.getFinalTransformation();
                pcl::PointCloud<pcl::PointXYZ>::Ptr transformed_cloud (new pcl::PointCloud<pcl::PointXYZ> ());
                pcl::transformPointCloud (*preAligned, *transformed_cloud, m);
pcl::io::savePLYFileBinary("cloud.ply", *b);
pcl::io::savePLYFileBinary("preCloud.ply", *preAligned);
pcl::io::savePLYFileBinary("ref.ply", *refCloud);

//XXX
 Eigen::Matrix4f prev;
  icp.setMaximumIterations (2);
  for (int j = 0; j < 30; ++j)
  {
    PCL_INFO ("Iteration No. %d.\n", j);

    icp.setInputSource (transformed_cloud);
    icp.align (res);
    m = icp.getFinalTransformation () * m;
    if (std::abs ((icp.getLastIncrementalTransformation () - prev).sum ()) < icp.getTransformationEpsilon ())
      icp.setMaxCorrespondenceDistance (icp.getMaxCorrespondenceDistance () - 0.001);
    
    prev = icp.getLastIncrementalTransformation ();
  }
                pcl::transformPointCloud (*b, *transformed_cloud, GLM2E(camMatrices[i])*m);
                pcl::io::savePLYFileBinary("cloudTF.ply", *transformed_cloud);
                camMatrices[i] = (camMatrices[i]*E2GLM(m));
//XXX

            }
            else
            {
                pcl::copyPointCloud(*cloud, *refCloud);
                camMatrices[i] = glm::mat4(1.0f);
            }
        }
     }
}

void initKPCDevice(int colorModeIndex, int depthModeIndex, vars::Vars&vars){
    const uint32_t deviceCount = k4a::device::get_installed_count();
    if (deviceCount < DEVICE_COUNT)
        throw std::runtime_error("Not enough Azure Kinect devices connected");

    k4a_device_configuration_t config = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
    config.depth_mode = depthModes[depthModeIndex];
    config.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32;
    config.color_resolution = colorModes[colorModeIndex];
    config.camera_fps = (config.color_resolution == K4A_COLOR_RESOLUTION_3072P || config.depth_mode == K4A_DEPTH_MODE_WFOV_UNBINNED) ? K4A_FRAMES_PER_SECOND_15 : K4A_FRAMES_PER_SECOND_30;
    config.synchronized_images_only = true;
    auto size = vars.reCreate<glm::ivec2>("depthSize"); 
    *size = getDepthResolution(config.depth_mode);

    for(int i=0; i<DEVICE_COUNT; i++)
    {
        std::string index = std::to_string(i);
        auto dev = vars.reCreate<k4a::device>("kinectDevice"+index);
        *dev = k4a::device::open(i);
        dev->start_cameras(&config);
        vars.reCreate<ge::gl::Texture>("colorTexture"+index,GL_TEXTURE_2D,GL_RGBA32F,1,size->x,size->y); 
        vars.reCreate<ge::gl::Texture>("depthTexture"+index,GL_TEXTURE_2D,GL_R16UI,1,size->x,size->y);
        camMatrices[i] = glm::mat4(1.0);
    }

    auto cal = vars.get<k4a::device>("kinectDevice0")->get_calibration(config.depth_mode, config.color_resolution);
    auto trans = vars.reCreate<k4a::transformation>("transformation",cal); 

    auto xyTable = vars.reCreate<k4a::image>("xyTable");
    *xyTable = k4a::image::create(K4A_IMAGE_FORMAT_CUSTOM, size->x, size->y, size->x * (int)sizeof(k4a_float2_t));
    createXyTable(&cal,xyTable);
    auto xyTexture = vars.reCreate<ge::gl::Texture>("xyTexture",GL_TEXTURE_2D,GL_RG32F,1,size->x,size->y);
    ge::gl::glTextureSubImage2D(xyTexture->getId(),0,0,0,size->x, size->y,GL_RG,GL_FLOAT,xyTable->get_buffer());
}

void initKPC(vars::Vars&vars){
    vars.addFloat("image.pointSize",1);
    addVarsLimitsF(vars,"image.pointSize",0.0f,10.0f);
    vars.addFloat("sampleRate", 1);
    addVarsLimitsF(vars,"sampleRate",0.1f,50.0f, 0.1);
    vars.add<glm::vec2>("clipRange", 0, 2000);
    initKPCDevice(colorModeIndex, depthModeIndex ,vars);
    vars.add<glm::vec3>("translate");
    addVarsLimitsF(vars,"translate",-50.0f,50.0f, 0.01);
    vars.add<glm::vec3>("rotate");
    addVarsLimitsF(vars,"rotate",-50.0f,50.0f, 0.001);
    vars.addBool("manual");
}

void updateKPC(vars::Vars&vars)
{
    if(vars.getBool("manual"))
    camMatrices[1] = glm::translate(glm::mat4(1.0), *vars.get<glm::vec3>("translate")) *
                     glm::rotate(glm::mat4(1.0), vars.get<glm::vec3>("rotate")->x, glm::vec3(1.0,0.0,0.0))*
                     glm::rotate(glm::mat4(1.0), vars.get<glm::vec3>("rotate")->y, glm::vec3(0.0,1.0,0.0))*
                     glm::rotate(glm::mat4(1.0), vars.get<glm::vec3>("rotate")->z, glm::vec3(0.0,0.0,1.0));
    for(int i=0; i<DEVICE_COUNT; i++)
    {
        std::string index = std::to_string(i);
        k4a::capture capture;
        auto dev = vars.get<k4a::device>("kinectDevice"+index);
        if (dev->get_capture(&capture, std::chrono::milliseconds(0)))
        {
            const k4a::image depthImage = capture.get_depth_image();
            const k4a::image colorImage = capture.get_color_image();

            auto size = vars.get<glm::ivec2>("depthSize");
            auto depthTexture = vars.get<ge::gl::Texture>("depthTexture"+index);
            auto colorTexture = vars.get<ge::gl::Texture>("colorTexture"+index);

            auto trans = vars.get<k4a::transformation>("transformation");
            k4a::image tImage = trans->color_image_to_depth_camera(depthImage, colorImage);
            ge::gl::glTextureSubImage2D(depthTexture->getId(),0,0,0,size->x, size->y,GL_RED_INTEGER,GL_UNSIGNED_SHORT,depthImage.get_buffer());
            ge::gl::glTextureSubImage2D(colorTexture->getId(),0,0,0,size->x, size->y,GL_BGRA,GL_UNSIGNED_BYTE,tImage.get_buffer());
        }
    }

    ImGui::Begin("vars");
    std::vector<const char*> colorModeList = { "720p", "2160p", "1440p", "1080p", "3072p", "1536p" };
    static int currentColor = 0;
    bool changed = false;
    if(ImGui::ListBox("Color mode", &currentColor, colorModeList.data(), colorModeList.size(), 4))
    {
        colorModeIndex = currentColor;
        changed = true;
    }
    std::vector<const char*> depthModeList = { "NFOV unbinned", "WFOV unbinned", "NFOV binned", "WFOV unbinned" };
    static int currentDepth = 1;
    if(ImGui::ListBox("Depth mode", &currentDepth, depthModeList.data(), depthModeList.size(), 4))
    {
        depthModeIndex = currentDepth;
        changed = true;
    }
    if(ImGui::Button("Align"))
        alignClouds(vars);

    ImGui::End();  
    if(changed)
        initKPCDevice(colorModeIndex, depthModeIndex, vars);
}

void drawKPC(vars::Vars&vars,glm::mat4 const&view,glm::mat4 const&proj){

    createKPCProgram(vars);
    vars.get<ge::gl::VertexArray>("emptyVao")->bind();

    auto xyTexture     = vars.get<ge::gl::Texture>("xyTexture");
    auto sampleRate = vars.getFloat("sampleRate");
    for(int i=0; i<DEVICE_COUNT; i++)
    {
        std::string index = std::to_string(i);
        auto colorTex = vars.get<ge::gl::Texture>("colorTexture"+index);
        auto depthTex = vars.get<ge::gl::Texture>("depthTexture"+index);
        colorTex->bind(i);
        depthTex->bind(i+DEVICE_COUNT);
    }
    xyTexture->bind(DEVICE_COUNT*2);
    vars.get<ge::gl::Program>("pointCloudProgram")
        ->setMatrix4fv("projView"      ,glm::value_ptr(proj*view))
        ->set2fv("clipRange",reinterpret_cast<float*>(vars.get<glm::vec2>("clipRange")))
        ->set1f("sampleRate", sampleRate)
        ->setMatrix4fv("camMatrices", glm::value_ptr(camMatrices[0]), DEVICE_COUNT) 
        ->use();

    auto size = vars.get<glm::ivec2>("depthSize");
    ge::gl::glEnable(GL_DEPTH_TEST);
    ge::gl::glPointSize(vars.getFloat("image.pointSize"));
    ge::gl::glDrawArrays(GL_POINTS,0,static_cast<int>(DEVICE_COUNT*size->x*size->y*sampleRate*sampleRate-1000));
    ge::gl::glDisable(GL_DEPTH_TEST);

    vars.get<ge::gl::VertexArray>("emptyVao")->unbind();
    for(int i=0; i<DEVICE_COUNT; i++)
    {
        std::string index = std::to_string(i);
        auto colorTex = vars.get<ge::gl::Texture>("colorTexture"+index);
        auto depthTex = vars.get<ge::gl::Texture>("depthTexture"+index);
        colorTex->unbind(i);
        depthTex->unbind(i+DEVICE_COUNT);
    }
    xyTexture->unbind(DEVICE_COUNT*2);
}

void drawKPC(vars::Vars&vars)
{
    auto view = vars.getReinterpret<basicCamera::CameraTransform>("view");
    auto projection = vars.get<basicCamera::PerspectiveCamera>("projection");
    drawKPC(vars,view->getView(),projection->getProjection());
}


