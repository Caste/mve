#include "objfile.h"

#include "util/exception.h"
#include "util/fs.h"

#include <iostream>
#include <fstream>

MVE_NAMESPACE_BEGIN
MVE_GEOM_NAMESPACE_BEGIN

MVE_GEOM_OBJ_NAMESPACE_BEGIN

/* ---------------------------------------------------------------- */

std::istream& 
safe_get_line(std::istream& is, std::string& t)
{
    t.clear();

    // The characters in the stream are read one-by-one using a std::streambuf.
    // That is faster than reading them one-by-one using the std::istream.
    // Code that uses streambuf this way must be guarded by a sentry object.
    // The sentry object performs various tasks,
    // such as thread synchronization and updating the stream state.

    std::istream::sentry se(is, true);
    std::streambuf* sb = is.rdbuf();

    for (;;)
    {
        int c = sb->sbumpc();
        switch (c)
        {
        case '\r':
            c = sb->sgetc();
            if (c == '\n')
                sb->sbumpc();
            return is;
        case '\n':
        case EOF:
            return is;
        default:
            t += (char) c;
        }
    }
}

/* ---------------------------------------------------------------- */

void 
Material::reset()
{
    identifier = "";
    diffuse_texture = "";

    math::Vec4f d(0.0f);
    diffuse = d;
    ambient = d;
    specular = d;
    emissive = d;
    shininess = 1.0f;
}

/* ---------------------------------------------------------------- */

MaterialLibrary::~MaterialLibrary()
{
    clear();
}

/* ---------------------------------------------------------------- */

void 
MaterialLibrary::clear()
{
    for (size_t i = 0; i < materials.size(); ++i)
    {
        delete materials[i];
    }

    materials.clear();
    material_ids.clear();
}

/* ---------------------------------------------------------------- */

Material * 
MaterialLibrary::get_material(unsigned int id) const
{
    if (id < materials.size())
    {
        return materials[id];
    }

    return NULL;
}

/* ---------------------------------------------------------------- */

int 
MaterialLibrary::get_material_idx_by_name(std::string const & name) const
{
    // lookup in material index map
    MaterialNameIdxMap::const_iterator idx = material_ids.find(name);

    if (idx != material_ids.end())
    {
        return (*idx).second;
    }
    else
    {
        return -1;
    }
}

/* ---------------------------------------------------------------- */

void 
MaterialLibrary::add_material(Material* mat)
{
    materials.push_back(mat);
    material_ids[mat->identifier] = materials.size() - 1;
}

/* ---------------------------------------------------------------- */

bool 
MaterialLibrary::load(std::string const & _file)
{
    try
    {
        std::ifstream mtl_file(_file.c_str());

        if (mtl_file.fail())
        {
            std::cerr << "MaterialLibrary: wasn't able to load '" << _file << "'" << std::endl;
            return false;
        }

        // Initial values for default material
        Material currMat;
        currMat.reset();

        std::string mat_name = "default";

        currMat.identifier = mat_name;

        // ensure default material is present
        if (get_material(0) == NULL)
        {
            add_material(new Material(currMat));
        }

        enum states
        {
            start = 1,
            read_newmtl,
            have_K,
            read_Kd,
            read_Ka,
            read_Ks,
            read_Ke,
            read_map_K,
            read_Ns
        };
        enum states currState = start;

        while (mtl_file)
        {
            switch (currState)
            {
            case start:
            {
                char c;
                mtl_file >> c;

                switch (c)
                {
                case 'K':
                    currState = have_K;
                    break;

                case 'n':
                    currState = read_newmtl;
                    break;

                case 'm':
                    currState = read_map_K;
                    break;

                case 'N':
                    currState = read_Ns;
                    break;

                default:
                    std::string temp;
                    safe_get_line(mtl_file, temp);
                    break;
                }
            }
            break;

            case have_K:
            {
                char c;
                mtl_file >> c;

                switch (c)
                {
                case 'a':
                    currState = read_Ka;
                    break;

                case 'd':
                    currState = read_Kd;
                    break;

                case 's':
                    currState = read_Ks;
                    break;

                case 'e':
                    currState = read_Ke;
                    break;

                default:
                    std::string temp;
                    safe_get_line(mtl_file, temp);
                    break;
                }
            }
            break;

            case read_Ka:
            {
                math::Vec4f vec;
                mtl_file >> vec[0];
                mtl_file >> vec[1];
                mtl_file >> vec[2];
                vec[3] = 1.0f;
                currMat.ambient = vec;
                currState = start;
            }
            break;

            case read_Ke:
            {
                math::Vec4f vec;
                mtl_file >> vec[0];
                mtl_file >> vec[1];
                mtl_file >> vec[2];
                vec[3] = 1.0f;
                currMat.emissive = vec;
                currState = start;
            }
            break;

            case read_Ns:
            {
                char c;
                mtl_file >> c;

                mtl_file >> currMat.shininess;
                currState = start;
            }
            break;

            case read_Kd:
            {
                math::Vec4f vec;
                mtl_file >> vec[0];
                mtl_file >> vec[1];
                mtl_file >> vec[2];
                vec[3] = 1.0f;
                currMat.diffuse = vec;
                currState = start;
            }
            break;

            case read_Ks:
            {
                math::Vec4f vec;
                mtl_file >> vec[0];
                mtl_file >> vec[1];
                mtl_file >> vec[2];
                vec[3] = 1.0f;
                currMat.specular = vec;
                currState = start;
            }
            break;

            case read_map_K:
            {
                std::string line;
                safe_get_line(mtl_file, line);
                std::string::size_type pos = line.find("ap_Kd", 0);

                if (pos == std::string::npos)
                {
                    std::cerr << "error parsing texture type, expected map_Kd but got: " << line << std::endl;
                }
                else
                {
                    std::string texFilename = line.substr(pos + 6);

                    currMat.diffuse_texture = texFilename;
                }

                currState = start;
            }
            break;

            case read_newmtl:
            {
                std::string line;
                safe_get_line(mtl_file, line);
                std::string::size_type pos = line.find("ewmtl", 0);

                if (pos == std::string::npos)
                {
                    std::cerr << "invalid expression: " << line << std::endl;
                }
                else
                {
                    if (mat_name != "default")
                    {
                        // push back old material
                        add_material(new Material(currMat));
                    }

                    currMat.reset();
                    mat_name = line.substr(pos + 6);
                    currMat.identifier = mat_name;
                }

                currState = start;
            }
            break;

            default:
                currState = start;
                break;
            }
        }

        // push back the last material:
        if (mat_name != "default")
        {
            add_material(new Material(currMat));
        }

        mtl_file.close();
        return true;
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception in ObjModel::loadMtlLib =" << e.what() << std::endl;
        return false;
    }
}

/* ---------------------------------------------------------------- */

math::Vec3f 
ObjModel::read_vec3 (std::ifstream& file) const
{
    math::Vec3f vec;
    file >> vec[0];
    file >> vec[1];
    file >> vec[2];
    return vec;
}

/* ---------------------------------------------------------------- */

math::Vec2f 
ObjModel::read_vec2 (std::ifstream& file) const
{
    math::Vec2f vec;
    file >> vec[0];
    file >> vec[1];
    return vec;
}

/* ---------------------------------------------------------------- */

bool
ObjModel::read_face (std::ifstream& file)
{
    char c = '/';
    unsigned int num_face_vertices = 0;
    
    std::vector<FatVertexIndex> face_vertices;
    face_vertices.resize(8);

    while (true)
    {
        do
        {
            file.get(c);
        }
        while (c == ' ' || c == '\t');

        file.unget();

        file.get(c);

        if (c != '\n' && c != '\r')
        {
            file.unget();
            FatVertexIndex fvi;
            fvi.vertex = -1;
            fvi.tex_coords = -1;
            fvi.normal = -1;

            file >> fvi.vertex;
            file.get(c);

            if (c == '/')
            {
                file.get(c);

                if (c != '/')
                {
                    file.unget();
                    file >> fvi.tex_coords;
                    file.get(c);
                }

                if (c == '/')
                {
                    file >> fvi.normal;
                }
            }

            face_vertices[num_face_vertices++] = fvi;
        }
        else
        {
            break;
        }
    }

    if (num_face_vertices < 3)
    {
        std::cerr << "Invalid face" << std::endl;
        return false;
    }

    // triangulate faces if #faces > 3!
    for (int i = 0; i < num_face_vertices - 2; ++i)
    {
        ModelFace f;
        f.vertices[0] = face_vertices[0].vertex;
        f.tex_coords[0] = face_vertices[0].tex_coords;
        f.normals[0] = face_vertices[0].normal;

        f.vertices[1] = face_vertices[i + 1].vertex;
        f.tex_coords[1] = face_vertices[i + 1].tex_coords;
        f.normals[1] = face_vertices[i + 1].normal;

        f.vertices[2] = face_vertices[i + 2].vertex;
        f.tex_coords[2] = face_vertices[i + 2].tex_coords;
        f.normals[2] = face_vertices[i + 2].normal;

        faces.push_back(f);
    }
    
    return true;
}

/* ---------------------------------------------------------------- */

bool
ObjModel::read_material (std::ifstream& file, MaterialLibrary & mtl_lib)
{
    std::string line;
    safe_get_line(file, line);
    std::string::size_type pos = line.find("semtl", 0);
    
    if (pos == std::string::npos)
    {
        std::cerr << "Error while parsing usemtl, got: " << line << std::endl;
        return false;
    }
    else
    {
        if (!groups.empty())
        {
            groups.back().end = faces.size();
        }
    
        Group g;
        g.start = faces.size();
        std::string usemtl = line.substr(pos + 6);
    
        int materialId = mtl_lib.get_material_idx_by_name(usemtl);
    
        if (materialId != -1)
        {
            g.material_id = materialId;
        }
        else
        {
            std::cerr << "Material not found: " << usemtl << std::endl;
            g.material_id = 0;
        }
    
        groups.push_back(g);
    }
    
    return true;
}

/* ---------------------------------------------------------------- */

bool
ObjModel::read_material_lib (MaterialLibrary & mtl_lib, std::string const & mtl_lib_prefix, 
                             std::ifstream & file)
{
    std::string line;
    safe_get_line(file, line);
    std::string::size_type pos = line.find("tllib", 0);

    if (pos == std::string::npos)
    {
        std::cerr << "Error while parsing material lib name" << std::endl;
        std::cerr << "Expected mtllib NAME, but got: " << line << std::endl;
    }
    else
    {
        // push back old material
        std::string mtl_lib_path = line.substr(pos + 6);
        material_lib_name = mtl_lib_prefix + '/' + mtl_lib_path;

        if (!mtl_lib.load(material_lib_name))
        {
            return false;
        }
    }
    
    return true;
}

/* ---------------------------------------------------------------- */

bool 
ObjModel::load(std::string const & _file,
                    std::string const & mtl_lib_prefix,
                    MaterialLibrary & mtl_lib)
{
    std::ifstream modelfile(_file.c_str());

    if (modelfile.fail())
    {
        std::cerr << "ObjModel: wasn't able to load file " << _file << std::endl;
        return false;
    }
    else
    {
        std::cout << "CObjModel: Loading " << _file << std::endl;
    }

    unsigned int lines_read = 0;

    enum states
    {
        start = 1, have_v, read_vt, read_vn, read_f, read_usemtl, read_mtllib
    };
    enum states curr_state = start;

    while (modelfile)
    {
        switch (curr_state)
        {
        case start:
        {
            lines_read++;
            char c;
            std::string temp;
            modelfile >> c;

            switch (c)
            {
            case 'v':
                curr_state = have_v;
                break; // go to differenciation between v, vt and vn

            case 'f':
                curr_state = read_f;
                break; // read face

            case 'm':
                curr_state = read_mtllib;
                break; // read in a materiallib

            case 'u':
                curr_state = read_usemtl;
                break; // use the specified material for this group

            case '\r':
                break; // skip white space without getline

            default:
                safe_get_line(modelfile, temp);
                break; // line which doesnt interest ous
            }
        }
        break;

        case have_v:
        {
            char c;
            modelfile.get(c);

            switch (c)
            {
            case 't':
                curr_state = read_vt;
                break;

            case 'n':
                curr_state = read_vn;
                break;

            default: // Read Vertex
                vertices.push_back(read_vec3(modelfile));
                curr_state = start;
                break;
            }
        }
        break;

        case read_vt: // Read Texture Coordinate
        {
            tex_coords.push_back(read_vec2(modelfile));
            curr_state = start;
        }
        break;

        case read_vn: // Read Vertex Normal
        {
            normals.push_back(read_vec3(modelfile));
            curr_state = start;
        }
        break;

        case read_f: // Read triangle
        {
            if(!read_face(modelfile))
                return false;
            curr_state = start;
        }
        break;

        case read_mtllib:
        {
            if(!read_material_lib(mtl_lib, mtl_lib_prefix, modelfile))
                return false;
            curr_state = start;
        }
        break;
            
        case read_usemtl:
        {
            if(!read_material(modelfile, mtl_lib))
                return false;
            curr_state = start;
        }
        break;
            
        default:
            curr_state = start;
            break;
        }
    }

    if (!groups.empty())
    {
        groups.back().end = faces.size();
    }
    else
    {
        Group g;
        g.start = 0;
        g.end = faces.size();
        g.material_id = 0;
        groups.push_back(g);

        Material current_material;
        current_material.reset();

        std::string mat_name = "default";
        current_material.identifier = mat_name;

        // ensure default material is present
        if (mtl_lib.size() == 0)
            mtl_lib.add_material(new Material(current_material));

    }

    std::cout << "ObjModel: finished loading mesh data, " <<
              faces.size() << " faces, " << groups.size() << " groups.";
    std::cout << "from " << vertices.size() << " vertices. (" << lines_read << " lines)" << std::endl;

    return true;
}

/* ---------------------------------------------------------------- */

std::pair<mve::TriangleMesh::Ptr, std::string> 
ObjModel::triangle_mesh_from_group(unsigned int idx, MaterialLibrary const & mat_lib)
{
    // read triangle material id
    unsigned int material_idx = groups[idx].material_id;
    Material const * group_material = mat_lib.get_material(material_idx);

    unsigned int num_faces = groups.at(idx).end - groups.at(idx).start;

    // prepare TriangleMesh
    mve::TriangleMesh::Ptr mesh = mve::TriangleMesh::create();
    mve::TriangleMesh::VertexList& mesh_vertices = mesh->get_vertices();
    mve::TriangleMesh::FaceList& mesh_faces = mesh->get_faces();
    mve::TriangleMesh::TexCoordList& tcoords = mesh->get_vertex_texcoords();
    mve::TriangleMesh::NormalList& vnormals = mesh->get_vertex_normals();

    vertices.reserve(num_faces * 3);
    faces.reserve(num_faces * 3);
    vnormals.reserve(num_faces * 3);

    // are texture coordinates specified?
    bool add_tex_coords = true;

    if (tex_coords.size() != vertices.size()
            || faces[groups.at(idx).start].tex_coords[0] == -1
            || faces[groups.at(idx).start].tex_coords[1] == -1
            || faces[groups.at(idx).start].tex_coords[2] == -1)
    {
        add_tex_coords = false;
    }
    else
    {
        tcoords.reserve(num_faces * 3);
    }

    unsigned int vertex_idx = 0;

    for (std::vector<ModelFace>::iterator it = faces.begin() + groups.at(idx).start;
            it != (faces.begin() + groups.at(idx).end);
            ++it)
    {
        // red triangle vertices
        vertices.push_back(mesh_vertices[(*it).vertices[0] - 1]);
        vertices.push_back(mesh_vertices[(*it).vertices[1] - 1]);
        vertices.push_back(mesh_vertices[(*it).vertices[2] - 1]);

        // read triangle normals
        if ((*it).normals[0] != -1 && (*it).normals[1] != -1 && (*it).normals[2] != -1)
        {
            normals.push_back(normals[(*it).normals[0] - 1]);
            normals.push_back(normals[(*it).normals[1] - 1]);
            normals.push_back(normals[(*it).normals[2] - 1]);
        }
        else
        {
            // no normal avaiable, set to face normal
            math::Vec3f d1 = mesh_vertices[(*it).vertices[1] - 1] - mesh_vertices[(*it).vertices[0] - 1];
            math::Vec3f d2 = mesh_vertices[(*it).vertices[2] - 1] - mesh_vertices[(*it).vertices[0] - 1];
            math::Vec3f normal = d1.cross(d2).normalized();

            vnormals.push_back(normal);
            vnormals.push_back(normal);
            vnormals.push_back(normal);
        }

        if (add_tex_coords)
        {
            tcoords.push_back(tex_coords[(*it).tex_coords[0] - 1]);
            tcoords.push_back(tex_coords[(*it).tex_coords[1] - 1]);
            tcoords.push_back(tex_coords[(*it).tex_coords[2] - 1]);
        }

        // store vertices and material id in buffers
        mesh_faces.push_back(vertex_idx++);
        mesh_faces.push_back(vertex_idx++);
        mesh_faces.push_back(vertex_idx++);
    }

    return std::make_pair(mesh, group_material->diffuse_texture);
}

MVE_GEOM_OBJ_NAMESPACE_END

TexturedMeshList
load_obj_meshes(std::string const& filename)
{
    TexturedMeshList meshes;

    // split path from filename
    std::string path = util::fs::get_path_component(filename);

    objmodel::ObjModel model;
    objmodel::MaterialLibrary material_lib;

    if (!model.load(filename, path, material_lib))
    {
        throw util::Exception("Invalid Obj Model");
    }

    for (unsigned int i = 0; i < model.get_num_groups(); ++i)
    {
        meshes.push_back(model.triangle_mesh_from_group(i, material_lib));
    }

    return meshes;
}

MVE_GEOM_NAMESPACE_END
MVE_NAMESPACE_END
