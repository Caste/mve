/*
 * OBJ file loading and writing functions.
 * Written by Carsten Haubold <carstenhaubold@googlemail.com>.
 */

#ifndef MVE_OBJ_FILE_HEADER
#define MVE_OBJ_FILE_HEADER

#include "defines.h"
#include "trianglemesh.h"

#include <map>
#include <vector>
#include <math/vector.h>

MVE_NAMESPACE_BEGIN
MVE_GEOM_NAMESPACE_BEGIN

MVE_GEOM_OBJ_NAMESPACE_BEGIN

/* load a string paying attention to line endings */
std::istream& safe_get_line(std::istream& is, std::string& t);

/* Material flags */
enum MaterialFlags
{
    MATERIAL_TRANSMITTING = (unsigned int)(1u << 31u),
    MATERIAL_SPECULAR = 1u << 30u,
    MATERIAL_EMISSIVE = 1u << 29u,
    MATERIAL_EMISSIVE_ENV = 1u << 28u,
    MATERIAL_FORCE_UNSIGNED_INT = 0xffffffffu
};

/* 
 * Materials describe reflection properties of surfaces.
 * They can have different BSDFs, colors and textures 
 */
class Material
{
public:
    Material()
    {
        reset();
    }

    /* the identifier for this material, as used in ObjModel */
    std::string identifier;

    /* texture for the diffuse chanel */
    std::string diffuse_texture;

    /* diffuse color */
    math::Vec4f diffuse;

    /* ambient color */
    math::Vec4f ambient;

    /* specular color */
    math::Vec4f specular;

    /* emissive color */
    math::Vec4f emissive;

    /* shininess exponent for the Blinn-Phong lighting model */
    float shininess;

public:
    /* reset the material parameters to completely absorbing */
    void reset ();

    /* determines if the material emits light */
    bool is_emitting () const
    {
        return emissive[0] > 0.0f || emissive[1] > 0.0f || emissive[2] > 0.0f;
    }
};

class MaterialLibrary
{
public:
    ~MaterialLibrary();

    /* returns the number of materials stored in this library */
    size_t size () const
    {
        return materials.size();
    }

    /* clears the material library */
    void clear ();

    /* retrieve a material from the scene's data storage
     * @param name the name of the material, as used in the model file
     * @return a pointer to the material
     */
    Material * get_material (unsigned int id) const;

    /* retrieve a material idx by material name
     * @param name the name of the material, as used in the model file
     * @return the material idx
     */
    int get_material_idx_by_name (std::string const & name) const;

    /* add a new material to the scene's material collection */
    void add_material (Material * mat);

    /* load a material library file */
    bool load (std::string const & _file);

private:
    /* A vector of loaded materials */
    std::vector<Material*> materials;

    typedef std::map<std::string, int> MaterialNameIdxMap;

    /* the material names linked to their index in CScene::m_materials */
    MaterialNameIdxMap material_ids;
};

/* 
 * The object model class loads a wavefront .obj model from disk 
 * and provides the material and triangle data to our scene
 */
class ObjModel
{
public:
    /* 
     * A fat vertex index contains indice for position, 
     * texture coordinates and normal 
     */
    struct FatVertexIndex
    {
        int vertex;
        int tex_coords;
        int normal;
    };

    /*
     * A model face contains indices into the list of 
     * vertices/normals/tex_coords, grouped by triangles
     */
    struct ModelFace
    {
        int vertices[3];
        int tex_coords[3];
        int normals[3];
    };

    /* A group represents a range of faces with the same material */
    struct Group
    {
        /* inclusive start index in face array */
        unsigned int start;
        /* exclusive end index in face array */
        unsigned int end;
        /* id of the associated material */
        unsigned int material_id;
    };


private:
    /* the list of vertices */
    std::vector<math::Vec3f> vertices;

    /* all tex_coords */
    std::vector<math::Vec2f> tex_coords;

    /* the list of normals (not necessarily smooth) */
    std::vector<math::Vec3f> normals;

    /* a list of all faces in this model */
    std::vector<ModelFace> faces;

    /* the groups in this model */
    std::vector<Group> groups;

    /* material library file name */
    std::string material_lib_name;

public:
    /* load the actual model - invokes loading the materials as well */
    bool load (std::string const & _file, std::string const & mtl_lib_prefix, MaterialLibrary & mtl_lib);

    /* get a triangle mesh and the corresponding texture from this mesh */
    std::pair<mve::TriangleMesh::Ptr, std::string> 
    triangle_mesh_from_group (unsigned int idx, MaterialLibrary const & mat_lib);

    /* 
     * @return the number of groups in this mesh, are used as submeshes with different materials */
    size_t get_num_groups ()
    {
        return groups.size();
    }

    /* Get the number of triangles */
    size_t get_num_triangles ()
    {
        return faces.size();
    }

    std::string get_material_lib_name ()
    {
        return material_lib_name;
    }
    
private:   
    bool read_material (std::ifstream& file, MaterialLibrary &mtl_lib);
    bool read_material_lib (MaterialLibrary &mtl_lib, const std::string &mtl_lib_prefix, 
                            std::ifstream &file);
    bool read_face(std::ifstream& file);
    math::Vec2f read_vec2 (std::ifstream& file) const;
    math::Vec3f read_vec3 (std::ifstream& file) const;
};

MVE_GEOM_OBJ_NAMESPACE_END

typedef std::pair<TriangleMesh::Ptr, std::string> TexturedMesh;
typedef std::vector<TexturedMesh> TexturedMeshList;

/**
 * Loads a set of triangle meshes from an obj file, grouped by material.
 * Returns a list of mesh-texture pairs
 */
TexturedMeshList
load_obj_meshes(std::string const& filename);

MVE_GEOM_NAMESPACE_END
MVE_NAMESPACE_END

#endif // MVE_OBJ_FILE_HEADER
