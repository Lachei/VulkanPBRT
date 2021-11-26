#include <vsgXchange/images.h>

#include <vsg/io/FileSystem.h>
#include <vsg/io/ObjectCache.h>

#include <cstring>

#include <iostream>
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfFrameBuffer.h>
#include <OpenEXR/ImfRgbaFile.h>
#include <OpenEXR/half.h>

using namespace vsgXchange;

class CPP_IStream: public Imf::IStream{
    public:
        CPP_IStream (std::istream& file, const char fileName[]):
            IStream (fileName), _file (file){}
        virtual bool    read (char c[], int n){
            _file.read(c, n);
            return !_file.eof();
        };
        virtual Imf::Int64   tellg (){
            return _file.tellg();
        };
        virtual void    seekg (Imf::Int64 pos){
            _file.seekg(pos);
        };
        virtual void    clear (){
            _file.clear();
        };
      private:
        std::istream&          _file;
};
class CPP_OStream: public Imf::OStream{
    public:
        CPP_OStream (std::ostream& file, const char fileName[]):
            OStream (fileName), _file (file){}
        virtual void	write (const char c[], int n){
            _file.write(c, n);
        };
        virtual Imf::Int64   tellp (){
            return _file.tellp();
        };
        virtual void    seekp (Imf::Int64 pos){
            _file.seekp(pos);
        };
        virtual void    clear (){
            _file.clear();
        };
      private:
        std::ostream&          _file;
};
class Array_IStream: public Imf::IStream{
    public:
        Array_IStream (const uint8_t* data, size_t size, const char fileName[]):
            IStream (fileName), data (data), size(size), curPlace(0){}
        virtual bool    read (char c[], int n){
            if(curPlace + n > size)
                throw Iex::InputExc ("Unexpected end of file.");
            std::copy_n(data + curPlace, n, c);
            curPlace += n;
            return curPlace != size;
        };
        virtual Imf::Int64   tellg (){
            return curPlace;
        };
        virtual void    seekg (Imf::Int64 pos){
            curPlace = 0;
        };
        virtual void    clear (){
        };
      private:
        const uint8_t* data;
        size_t size;
        size_t curPlace;
};

static vsg::ref_ptr<vsg::Object> parseOpenExr(Imf::InputFile& file){
    Imath::Box2i dw = file.header().dataWindow();
    int width = dw.max.x - dw.min.x + 1;
    int height = dw.max.y - dw.min.y + 1;
    auto begin = file.header().channels().begin();
    int channelCount = 0; while(begin != file.header().channels().end()) {++begin; channelCount++;}
    if (channelCount > 4) return {};

    //single element half precision float
    if (channelCount == 1 && file.header().channels().begin().channel().type == Imf::HALF){
        uint16_t* pixels = new uint16_t[width * height];
        Imf::FrameBuffer frameBuffer;
        frameBuffer.insert(file.header().channels().begin().name(),
                        Imf::Slice(Imf::HALF, (char*)(pixels - dw.min.x - dw.min.y * width),
                        sizeof(pixels[0]),
                        sizeof(pixels[0]) * width));
        file.setFrameBuffer(frameBuffer);
        file.readPixels(dw.min.y, dw.max.y);
        return vsg::ushortArray2D::create(width, height, pixels, vsg::Data::Layout{VK_FORMAT_R16_SFLOAT});
    }

    //single element single precision float
    if (channelCount == 1 && file.header().channels().begin().channel().type == Imf::FLOAT){
        float* pixels = new float[width * height];
        Imf::FrameBuffer frameBuffer;
        frameBuffer.insert(file.header().channels().begin().name(),
                        Imf::Slice(Imf::FLOAT, (char*)(pixels - dw.min.x - dw.min.y * width),
                        sizeof(pixels[0]),
                        sizeof(pixels[0]) * width));
        file.setFrameBuffer(frameBuffer);
        file.readPixels(dw.min.y, dw.max.y);
        return vsg::floatArray2D::create(width, height, pixels, vsg::Data::Layout{VK_FORMAT_R32_SFLOAT});
    }

    //single element single precision uint
    if (channelCount == 1 && file.header().channels().begin().channel().type == Imf::UINT){
        uint32_t* pixels = new uint32_t[width * height];
        Imf::FrameBuffer frameBuffer;
        frameBuffer.insert(file.header().channels().begin().name(),
                        Imf::Slice(Imf::UINT, (char*)(pixels - dw.min.x - dw.min.y * width),
                        sizeof(pixels[0]),
                        sizeof(pixels[0]) * width));
        file.setFrameBuffer(frameBuffer);
        file.readPixels(dw.min.y, dw.max.y);
        return vsg::uintArray2D::create(width, height, pixels, vsg::Data::Layout{VK_FORMAT_R32_UINT});
    }

    //4 elements half precision float
    if (file.header().channels().begin().channel().type == Imf::HALF){
        vsg::usvec4* pixels = new vsg::usvec4[width * height];
        Imf::FrameBuffer frameBuffer;
        frameBuffer.insert("R",
                        Imf::Slice(Imf::HALF, (char*)(&(pixels - dw.min.x - dw.min.y * width)->r),
                        sizeof(pixels[0]),
                        sizeof(pixels[0]) * width));
        frameBuffer.insert("G",
                        Imf::Slice(Imf::HALF, (char*)(&(pixels - dw.min.x - dw.min.y * width)->g),
                        sizeof(pixels[0]),
                        sizeof(pixels[0]) * width));
        frameBuffer.insert("B",
                        Imf::Slice(Imf::HALF, (char*)(&(pixels - dw.min.x - dw.min.y * width)->b),
                        sizeof(pixels[0]),
                        sizeof(pixels[0]) * width));
        frameBuffer.insert("A",
                        Imf::Slice(Imf::HALF, (char*)(&(pixels - dw.min.x - dw.min.y * width)->a),
                        sizeof(pixels[0]),
                        sizeof(pixels[0]) * width));
        file.setFrameBuffer(frameBuffer);
        file.readPixels(dw.min.y, dw.max.y);
        return vsg::usvec4Array2D::create(width, height, pixels, vsg::Data::Layout{VK_FORMAT_R16G16B16A16_SFLOAT});
    }

    //4 elements single precision float
    if (file.header().channels().begin().channel().type == Imf::FLOAT){
        vsg::vec4* pixels = new vsg::vec4[width * height];
        Imf::FrameBuffer frameBuffer;
        frameBuffer.insert("R",
                        Imf::Slice(Imf::FLOAT, (char*)(&(pixels - dw.min.x - dw.min.y * width)->r),
                        sizeof(pixels[0]),
                        sizeof(pixels[0]) * width));

        frameBuffer.insert("G",
                        Imf::Slice(Imf::FLOAT, (char*)(&(pixels - dw.min.x - dw.min.y * width)->g),
                        sizeof(pixels[0]),
                        sizeof(pixels[0]) * width));
        frameBuffer.insert("B",
                        Imf::Slice(Imf::FLOAT, (char*)(&(pixels - dw.min.x - dw.min.y * width)->b),
                        sizeof(pixels[0]),
                        sizeof(pixels[0]) * width));
        frameBuffer.insert("A",
                        Imf::Slice(Imf::FLOAT, (char*)(&(pixels - dw.min.x - dw.min.y * width)->a),
                        sizeof(pixels[0]),
                        sizeof(pixels[0]) * width));
        file.setFrameBuffer(frameBuffer);
        file.readPixels(dw.min.y, dw.max.y);
        return vsg::vec4Array2D::create(width, height, pixels, vsg::Data::Layout{VK_FORMAT_R32G32B32A32_SFLOAT});
    }

    //4 elements single precision uint
    if (file.header().channels().begin().channel().type == Imf::UINT){
        vsg::uivec4* pixels = new vsg::uivec4[width * height];
        Imf::FrameBuffer frameBuffer;
        frameBuffer.insert("R",
                        Imf::Slice(Imf::UINT, (char*)(&(pixels - dw.min.x - dw.min.y * width)->r),
                        sizeof(pixels[0]),
                        sizeof(pixels[0]) * width));
        frameBuffer.insert("G",
                        Imf::Slice(Imf::UINT, (char*)(&(pixels - dw.min.x - dw.min.y * width)->g),
                        sizeof(pixels[0]),
                        sizeof(pixels[0]) * width));
        frameBuffer.insert("B",
                        Imf::Slice(Imf::UINT, (char*)(&(pixels - dw.min.x - dw.min.y * width)->b),
                        sizeof(pixels[0]),
                        sizeof(pixels[0]) * width));
        frameBuffer.insert("A",
                        Imf::Slice(Imf::UINT, (char*)(&(pixels - dw.min.x - dw.min.y * width)->a),
                        sizeof(pixels[0]),
                        sizeof(pixels[0]) * width));
        file.setFrameBuffer(frameBuffer);
        file.readPixels(dw.min.y, dw.max.y);
        return vsg::uivec4Array2D::create(width, height, pixels, vsg::Data::Layout{VK_FORMAT_R32G32B32A32_UINT});
    }

    return {};
}

openexr::openexr() :
    _supportedExtensions{"exr"}
{
}

vsg::ref_ptr<vsg::Object> openexr::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    if (const auto ext = vsg::lowerCaseFileExtension(filename); _supportedExtensions.count(ext) == 0)
    {
        return {};
    }

    vsg::Path filenameToUse = findFile(filename, options);
    if (filenameToUse.empty()) return {};

    int width, height, channels;
    Imf::InputFile file(filenameToUse.c_str());
    
    return parseOpenExr(file);
}

vsg::ref_ptr<vsg::Object> openexr::read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options) const
{
    if (!options || _supportedExtensions.count(options->extensionHint) == 0)
        return {};
    CPP_IStream stream(fin, "");
    Imf::InputFile file(stream);

    return parseOpenExr(file);
}

vsg::ref_ptr<vsg::Object> openexr::read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options) const
{
    if (!options || _supportedExtensions.count(options->extensionHint) == 0)
        return {};

    Array_IStream stream(ptr, size, "");
    Imf::InputFile file(stream);

    return parseOpenExr(file);
}

bool openexr::write(const vsg::Object* object, const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    if(const vsg::ushortArray2D* obj = dynamic_cast<const vsg::ushortArray2D*>(object)){ //single precision half float
        int width = obj->width(), height = obj->height();
        Imf::Header header(width, height);
        header.channels().insert("Y", Imf::Channel(Imf::HALF));

        Imf::OutputFile file(filename.c_str(), header);
        Imf::FrameBuffer frameBuffer;
        frameBuffer.insert("Y", Imf::Slice(Imf::HALF,
                            (char*)obj->data(), 
                            sizeof(*obj->data()), 
                            sizeof(*obj->data()) * width));
        file.setFrameBuffer(frameBuffer);
        file.writePixels(height);
        return true;
    }
    if(const vsg::floatArray2D* obj = dynamic_cast<const vsg::floatArray2D*>(object)){ //single precision float
        int width = obj->width(), height = obj->height();
        Imf::Header header(width, height);
        header.channels().insert("Y", Imf::Channel(Imf::FLOAT));

        int dataSize = obj->dataSize();
        int space = width * height * sizeof(float);
        int size = sizeof(*obj->data());
        float dat = obj->data()[0];
        Imf::OutputFile file(filename.c_str(), header);
        Imf::FrameBuffer frameBuffer;
        frameBuffer.insert("Y", Imf::Slice(Imf::FLOAT,
                            (char*)obj->data(), 
                            sizeof(*obj->data()), 
                            sizeof(*obj->data()) * width));
        file.setFrameBuffer(frameBuffer);
        file.writePixels(height);
        return true;
    }
    if(const vsg::uintArray2D* obj = dynamic_cast<const vsg::uintArray2D*>(object)){ //single precision uint
        int width = obj->width(), height = obj->height();
        Imf::Header header(width, height);
        header.channels().insert("Y", Imf::Channel(Imf::UINT));

        Imf::OutputFile file(filename.c_str(), header);
        Imf::FrameBuffer frameBuffer;
        frameBuffer.insert("Y", Imf::Slice(Imf::UINT,
                            (char*)obj->data(), 
                            sizeof(*obj->data()), 
                            sizeof(*obj->data()) * width));
        file.setFrameBuffer(frameBuffer);
        file.writePixels(height);
        return true;
    }
    if(const vsg::usvec4Array2D* obj = dynamic_cast<const vsg::usvec4Array2D*>(object)){ //single precision short
        int width = obj->width(), height = obj->height();
        Imf::Header header(width, height);
        header.channels().insert("R", Imf::Channel(Imf::HALF));
        header.channels().insert("G", Imf::Channel(Imf::HALF));
        header.channels().insert("B", Imf::Channel(Imf::HALF));
        header.channels().insert("A", Imf::Channel(Imf::HALF));

        Imf::OutputFile file(filename.c_str(), header);
        Imf::FrameBuffer frameBuffer;
        frameBuffer.insert("R", Imf::Slice(Imf::HALF,
                            (char*)(&obj->data()->r), 
                            sizeof(*obj->data()), 
                            sizeof(*obj->data()) * width));
        frameBuffer.insert("G", Imf::Slice(Imf::HALF,
                            (char*)(&obj->data()->g), 
                            sizeof(*obj->data()), 
                            sizeof(*obj->data()) * width));
        frameBuffer.insert("B", Imf::Slice(Imf::HALF,
                            (char*)(&obj->data()->b), 
                            sizeof(*obj->data()), 
                            sizeof(*obj->data()) * width));
        frameBuffer.insert("A", Imf::Slice(Imf::HALF,
                            (char*)(&obj->data()->a), 
                            sizeof(*obj->data()), 
                            sizeof(*obj->data()) * width));
        file.setFrameBuffer(frameBuffer);
        file.writePixels(height);
        return true;
    }
    if(const vsg::vec4Array2D* obj = dynamic_cast<const vsg::vec4Array2D*>(object)){ //single precision float
        int width = obj->width(), height = obj->height();
        Imf::Header header(width, height);
        header.channels().insert("R", Imf::Channel(Imf::FLOAT));
        header.channels().insert("G", Imf::Channel(Imf::FLOAT));
        header.channels().insert("B", Imf::Channel(Imf::FLOAT));
        header.channels().insert("A", Imf::Channel(Imf::FLOAT));

        Imf::OutputFile file(filename.c_str(), header);
        Imf::FrameBuffer frameBuffer;
        frameBuffer.insert("R", Imf::Slice(Imf::FLOAT,
                            (char*)(&obj->data()->r), 
                            sizeof(*obj->data()), 
                            sizeof(*obj->data()) * width));
        frameBuffer.insert("G", Imf::Slice(Imf::FLOAT,
                            (char*)(&obj->data()->g), 
                            sizeof(*obj->data()), 
                            sizeof(*obj->data()) * width));
        frameBuffer.insert("B", Imf::Slice(Imf::FLOAT,
                            (char*)(&obj->data()->b), 
                            sizeof(*obj->data()), 
                            sizeof(*obj->data()) * width));
        frameBuffer.insert("A", Imf::Slice(Imf::FLOAT,
                            (char*)(&obj->data()->a), 
                            sizeof(*obj->data()), 
                            sizeof(*obj->data()) * width));
        file.setFrameBuffer(frameBuffer);
        file.writePixels(height);
        return true;
    }
    if(const vsg::uivec4Array2D* obj = dynamic_cast<const vsg::uivec4Array2D*>(object)){ //single precision uint
        int width = obj->width(), height = obj->height();
        Imf::Header header(width, height);
        header.channels().insert("R", Imf::Channel(Imf::UINT));
        header.channels().insert("G", Imf::Channel(Imf::UINT));
        header.channels().insert("B", Imf::Channel(Imf::UINT));
        header.channels().insert("A", Imf::Channel(Imf::UINT));

        Imf::OutputFile file(filename.c_str(), header);
        Imf::FrameBuffer frameBuffer;
        frameBuffer.insert("R", Imf::Slice(Imf::UINT,
                            (char*)(&obj->data()->r), 
                            sizeof(*obj->data()), 
                            sizeof(*obj->data()) * width));
        frameBuffer.insert("G", Imf::Slice(Imf::UINT,
                            (char*)(&obj->data()->g), 
                            sizeof(*obj->data()), 
                            sizeof(*obj->data()) * width));
        frameBuffer.insert("B", Imf::Slice(Imf::UINT,
                            (char*)(&obj->data()->b), 
                            sizeof(*obj->data()), 
                            sizeof(*obj->data()) * width));
        frameBuffer.insert("A", Imf::Slice(Imf::UINT,
                            (char*)(&obj->data()->a), 
                            sizeof(*obj->data()), 
                            sizeof(*obj->data()) * width));
        file.setFrameBuffer(frameBuffer);
        file.writePixels(height);
        return true;
    }
    return false;
}

bool openexr::write(const vsg::Object* object, std::ostream& fout, vsg::ref_ptr<const vsg::Options> options) const
{
    if(const vsg::ushortArray2D* obj = dynamic_cast<const vsg::ushortArray2D*>(object)){ //single precision half float
        int width = obj->width(), height = obj->height();
        Imf::Header header(width, height);
        header.channels().insert("Z", Imf::Channel(Imf::HALF));

        CPP_OStream stream(fout, "");
        Imf::OutputFile file(stream, header);
        Imf::FrameBuffer frameBuffer;
        frameBuffer.insert("Z", Imf::Slice(Imf::HALF,
                            (char*)obj->data(), 
                            sizeof(*obj->data()), 
                            sizeof(*obj->data()) * width));
        file.setFrameBuffer(frameBuffer);
        file.writePixels(height);
        return true;
    }
    if(const vsg::floatArray2D* obj = dynamic_cast<const vsg::floatArray2D*>(object)){ //single precision float
        int width = obj->width(), height = obj->height();
        Imf::Header header(width, height);
        header.channels().insert("Z", Imf::Channel(Imf::FLOAT));

        CPP_OStream stream(fout, "");
        Imf::OutputFile file(stream, header);
        Imf::FrameBuffer frameBuffer;
        frameBuffer.insert("Z", Imf::Slice(Imf::FLOAT,
                            (char*)obj->data(), 
                            sizeof(*obj->data()), 
                            sizeof(*obj->data()) * width));
        file.setFrameBuffer(frameBuffer);
        file.writePixels(height);
        return true;
    }
    if(const vsg::uintArray2D* obj = dynamic_cast<const vsg::uintArray2D*>(object)){ //single precision uint
        int width = obj->width(), height = obj->height();
        Imf::Header header(width, height);
        header.channels().insert("Z", Imf::Channel(Imf::UINT));

        CPP_OStream stream(fout, "");
        Imf::OutputFile file(stream, header);
        Imf::FrameBuffer frameBuffer;
        frameBuffer.insert("Z", Imf::Slice(Imf::UINT,
                            (char*)obj->data(), 
                            sizeof(*obj->data()), 
                            sizeof(*obj->data()) * width));
        file.setFrameBuffer(frameBuffer);
        file.writePixels(height);
        return true;
    }
    if(const vsg::usvec4Array2D* obj = dynamic_cast<const vsg::usvec4Array2D*>(object)){ //single precision short
        int width = obj->width(), height = obj->height();
        Imf::Header header(width, height);
        header.channels().insert("R", Imf::Channel(Imf::HALF));
        header.channels().insert("G", Imf::Channel(Imf::HALF));
        header.channels().insert("B", Imf::Channel(Imf::HALF));
        header.channels().insert("A", Imf::Channel(Imf::HALF));

        CPP_OStream stream(fout, "");
        Imf::OutputFile file(stream, header);
        Imf::FrameBuffer frameBuffer;
        frameBuffer.insert("R", Imf::Slice(Imf::HALF,
                            (char*)(&obj->data()->r), 
                            sizeof(*obj->data()), 
                            sizeof(*obj->data()) * width));
        frameBuffer.insert("G", Imf::Slice(Imf::HALF,
                            (char*)(&obj->data()->g), 
                            sizeof(*obj->data()), 
                            sizeof(*obj->data()) * width));
        frameBuffer.insert("B", Imf::Slice(Imf::HALF,
                            (char*)(&obj->data()->b), 
                            sizeof(*obj->data()), 
                            sizeof(*obj->data()) * width));
        frameBuffer.insert("A", Imf::Slice(Imf::HALF,
                            (char*)(&obj->data()->a), 
                            sizeof(*obj->data()), 
                            sizeof(*obj->data()) * width));
        file.setFrameBuffer(frameBuffer);
        file.writePixels(height);
        return true;
    }
    if(const vsg::vec4Array2D* obj = dynamic_cast<const vsg::vec4Array2D*>(object)){ //single precision float
        int width = obj->width(), height = obj->height();
        Imf::Header header(width, height);
        header.channels().insert("R", Imf::Channel(Imf::FLOAT));
        header.channels().insert("G", Imf::Channel(Imf::FLOAT));
        header.channels().insert("B", Imf::Channel(Imf::FLOAT));
        header.channels().insert("A", Imf::Channel(Imf::FLOAT));

        CPP_OStream stream(fout, "");
        Imf::OutputFile file(stream, header);
        Imf::FrameBuffer frameBuffer;
        frameBuffer.insert("R", Imf::Slice(Imf::FLOAT,
                            (char*)(&obj->data()->r), 
                            sizeof(*obj->data()), 
                            sizeof(*obj->data()) * width));
        frameBuffer.insert("G", Imf::Slice(Imf::FLOAT,
                            (char*)(&obj->data()->g), 
                            sizeof(*obj->data()), 
                            sizeof(*obj->data()) * width));
        frameBuffer.insert("B", Imf::Slice(Imf::FLOAT,
                            (char*)(&obj->data()->b), 
                            sizeof(*obj->data()), 
                            sizeof(*obj->data()) * width));
        frameBuffer.insert("A", Imf::Slice(Imf::FLOAT,
                            (char*)(&obj->data()->a), 
                            sizeof(*obj->data()), 
                            sizeof(*obj->data()) * width));
        file.setFrameBuffer(frameBuffer);
        file.writePixels(height);
        return true;
    }
    if(const vsg::uivec4Array2D* obj = dynamic_cast<const vsg::uivec4Array2D*>(object)){ //single precision uint
        int width = obj->width(), height = obj->height();
        Imf::Header header(width, height);
        header.channels().insert("R", Imf::Channel(Imf::UINT));
        header.channels().insert("G", Imf::Channel(Imf::UINT));
        header.channels().insert("B", Imf::Channel(Imf::UINT));
        header.channels().insert("A", Imf::Channel(Imf::UINT));

        CPP_OStream stream(fout, "");
        Imf::OutputFile file(stream, header);
        Imf::FrameBuffer frameBuffer;
        frameBuffer.insert("R", Imf::Slice(Imf::UINT,
                            (char*)(&obj->data()->r), 
                            sizeof(*obj->data()), 
                            sizeof(*obj->data()) * width));
        frameBuffer.insert("G", Imf::Slice(Imf::UINT,
                            (char*)(&obj->data()->g), 
                            sizeof(*obj->data()), 
                            sizeof(*obj->data()) * width));
        frameBuffer.insert("B", Imf::Slice(Imf::UINT,
                            (char*)(&obj->data()->b), 
                            sizeof(*obj->data()), 
                            sizeof(*obj->data()) * width));
        frameBuffer.insert("A", Imf::Slice(Imf::UINT,
                            (char*)(&obj->data()->a), 
                            sizeof(*obj->data()), 
                            sizeof(*obj->data()) * width));
        file.setFrameBuffer(frameBuffer);
        file.writePixels(height);
        return true;
    }
    return false;
}

bool openexr::getFeatures(Features& features) const
{
    for (auto& ext : _supportedExtensions)
    {
        features.extensionFeatureMap[ext] = static_cast<vsg::ReaderWriter::FeatureMask>(vsg::ReaderWriter::READ_FILENAME | vsg::ReaderWriter::READ_ISTREAM | vsg::ReaderWriter::READ_MEMORY);
    }
    return true;
}