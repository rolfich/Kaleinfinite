#if defined(VERTEX)

in vec3 VertexPosition;
uniform int PassIndex;

out vData {
    vec2 uv;
    vec3 position;
}vertex;

void main(void) {
    vertex.uv = VertexPosition.xy * 0.5 + 0.5;
    gl_Position = vec4(VertexPosition.xy, 0.0, 1.0);
    vertex.position = VertexPosition;
}

#endif

#if defined(FRAGMENT)

uniform mat4 InverseViewProjection;
uniform mat4 Projection;
uniform mat4 View;
uniform vec3 CameraPosition;
uniform sampler2D FBOTexture;
uniform sampler2D Depth;
uniform float XKalei;
uniform float YKalei;
uniform float YLargeKalei;
uniform float ZKalei;
uniform int PassIndex;

in vData {
    vec2 uv;
    vec3 position;
} frag;

out vec4 Color;

struct Plane {
    vec3 p1;
    vec3 p2;
    vec3 p3;
    vec3 p4;
    vec3 n;
};

struct Prism {
    Plane p1;
    Plane p2;
    Plane p3;
    Plane texturePlane;
};

struct Ray {
    vec3 start;
    vec3 direction;
    int idxPlaneCollisioned;
};

Prism createPrism(){
    Prism prism;

    Plane texturePlane; // DEFE
    Plane p1; //ACDF
    Plane p2; //CBFE
    Plane p3; //BAED

    float xKalei = 0;
    float zKaleiA = 0;
    float zKaleiB = 0;

    switch(PassIndex){
        case 0:
            xKalei = XKalei;
            zKaleiA = ZKalei;
            zKaleiB = ZKalei;
            break;
        case 1:
            xKalei = 2*XKalei;
            zKaleiA = - ZKalei;
            zKaleiB = - ZKalei*3.;
            break;
        case 2:
            xKalei = 4*XKalei;
            zKaleiA = 3*ZKalei;
            zKaleiB = 5*ZKalei;
            break;
        case 3:
            xKalei = 8*XKalei;
            zKaleiA = - 5*ZKalei;
            zKaleiB = - 11*ZKalei;
            break;
        case 4:
            xKalei = 16*XKalei;
            zKaleiA = 11*ZKalei;
            zKaleiB = 21*ZKalei;
            break;
        case 5:
            xKalei = 32*XKalei;
            zKaleiA = - 21*ZKalei;
            zKaleiB = - 43*ZKalei;
            break;
        case 6:
            xKalei = 64*XKalei;
            zKaleiA = 43*ZKalei;
            zKaleiB = 85*ZKalei;
            break;
    }

    texturePlane.p1 = vec3(-xKalei, -YKalei, zKaleiA);
    texturePlane.p2 = vec3(0.0, -YKalei, -zKaleiB);
    texturePlane.p3 = vec3(xKalei, -YKalei, zKaleiA);
    texturePlane.p4 = vec3(0.0, -YKalei, -zKaleiB);

    p1.p1 = vec3(-xKalei, YLargeKalei, zKaleiA);
    p1.p2 = vec3(xKalei, YLargeKalei, zKaleiA);
    p1.p3 = vec3(-xKalei, -YKalei, zKaleiA);
    p1.p4 = vec3(xKalei, -YKalei, zKaleiA);
   
    p2.p1 = vec3(xKalei, YLargeKalei, zKaleiA);
    p2.p2 = vec3(0.0, YLargeKalei, -zKaleiB);
    p2.p3 = vec3(xKalei, -YKalei, zKaleiA);
    p2.p4 = vec3(0.0, -YKalei, -zKaleiB);

    p3.p1 = vec3(0.0, YLargeKalei, -zKaleiB);
    p3.p2 = vec3(-xKalei, YLargeKalei, zKaleiA);
    p3.p3 = vec3(0.0, -YKalei, -zKaleiB);
    p3.p4 = vec3(-xKalei, -YKalei, zKaleiA);

    if(PassIndex%2 == 0){
        texturePlane.n = normalize(vec3(0.0, 1.0, 0.0));
        p1.n = normalize(vec3(0.0, 0.0, -1.0));
        p2.n = normalize(vec3(-1.5, 0, sqrt(3.0)/2));
        p3.n = normalize(vec3(1.5, 0, sqrt(3.0)/2));
    } else {
        texturePlane.n = normalize(vec3(0.0, 1.0, 0.0));
        p1.n = normalize(vec3(0.0, 0.0, 1.0));
        p2.n = normalize(vec3(-1.5, 0, -sqrt(3.0)/2));
        p3.n = normalize(vec3(1.5, 0, -sqrt(3.0)/2));
    }

    prism.texturePlane = texturePlane;
    prism.p1 = p1;
    prism.p2 = p2;
    prism.p3 = p3;
    
    return prism;
}

// Returns the area of a triangle with the heron formula
float triangleArea(vec3 p1,vec3 p2,vec3 p3)
{
    float a=sqrt((p2.x-p1.x)*(p2.x-p1.x)+(p2.y-p1.y)*(p2.y-p1.y)+(p2.z-p1.z)*(p2.z-p1.z));
    float b=sqrt((p3.x-p2.x)*(p3.x-p2.x)+(p3.y-p2.y)*(p3.y-p2.y)+(p3.z-p2.z)*(p3.z-p2.z));
    float c=sqrt((p1.x-p3.x)*(p1.x-p3.x)+(p1.y-p3.y)*(p1.y-p3.y)+(p1.z-p3.z)*(p1.z-p3.z));
    float s=(a+b+c)/2.;
    return (sqrt(s*((s-a)*(s-b)*(s-c))));
}

// Returns the norm of a vec3
float norm(vec3 v){
    return sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
}

vec3 computeCollisionPoint(Ray r, Plane p){

    float a = r.direction.x * p.n.x + r.direction.y * p.n.y + r.direction.z * p.n.z;
    
    if(a == 0) {
        // The ray is colinear to the plane
        return vec3(-1000., -1000., -1000.);
    }
            
    float t = (( p.p1.x * p.n.x + p.p1.y * p.n.y + p.p1.z * p.n.z - p.n.x * r.start.x - p.n.y * r.start.y - p.n.z * r.start.z ) / a );
    if(t < 0) {
        // The ray goes away from the plane
        return vec3(-1000., -1000., -1000.);
    }

    float x = r.start.x + t * r.direction.x;
    float y = r.start.y + t * r.direction.y;
    float z = r.start.z + t * r.direction.z;
    vec3 cp = vec3(x,y,z);

    return cp;
}

bool testAreas(vec3 cp, Plane p) {
    if(abs(triangleArea(p.p2,p.p3,p.p4)-triangleArea(p.p2,p.p4,cp)-triangleArea(p.p2,p.p3,cp)-triangleArea(p.p3,p.p4,cp)) < 0.1 || abs(triangleArea(p.p1,p.p2,p.p3)-triangleArea(p.p1,p.p2,cp)-triangleArea(p.p2,p.p3,cp)-triangleArea(p.p1,p.p3,cp)) < 0.1) {
        return true;
    }else return false;

}

// Returns reflection of the ray if it has collisionned
// Returns the ray if not, with a -1 index
Ray rayMirror(Ray r, Plane p1, Plane p2, Plane p3, Plane texturePlane) {

    Ray nullRay = r;
    nullRay.idxPlaneCollisioned = -1;

    int index = r.idxPlaneCollisioned;
    r.idxPlaneCollisioned = -1;
    
    // Test on plane 1
    if(index != 1) {
        vec3 cp = computeCollisionPoint(r, p1);
        if(cp.x != -1000.) {
            // Collision with plane p1, testing the collision with the quad
            if(testAreas(cp, p1)) {
                // Collision with the mirror 1, update ray
                r.idxPlaneCollisioned = 1;
                r.start = cp;
                r.direction = normalize(vec3(-2 * p1.n.x * dot(p1.n, r.direction) + r.direction.x, 
                                -2 * p1.n.y * dot(p1.n, r.direction) + r.direction.y,
                                -2 * p1.n.z * dot(p1.n, r.direction) + r.direction.z));
                return r;
            }
        }
    }

    if(index != 2 && r.idxPlaneCollisioned != 1) {
        // Test on plane 2
        vec3 cp = computeCollisionPoint(r, p2);
        if(cp.x != -1000.) {
            // Collision with plane p2, testing the collision with the quad
            if(testAreas(cp, p2)) {
                // Collision with the mirror 2, update ray
                r.idxPlaneCollisioned = 2;
                r.start = cp;
                r.direction = normalize(vec3(-2 * p2.n.x * dot(p2.n, r.direction) + r.direction.x, 
                                -2 * p2.n.y * dot(p2.n, r.direction) + r.direction.y,
                                -2 * p2.n.z * dot(p2.n, r.direction) + r.direction.z));
                return r;
            }
        }
    }

    if(index != 3 && r.idxPlaneCollisioned != 1 && r.idxPlaneCollisioned != 2) {
        // Test on plane 3
        vec3 cp = computeCollisionPoint(r, p3);
        if(cp.x != -1000.) {
            // Collision with plane p3, testing the collision with the quad
            if(testAreas(cp, p3)) {
                // Collision with the mirror 3, update ray
                r.idxPlaneCollisioned = 3;
                r.start = cp;
                r.direction = normalize(vec3(-2 * p3.n.x * dot(p3.n, r.direction) + r.direction.x, 
                                -2 * p3.n.y * dot(p3.n, r.direction) + r.direction.y,
                                -2 * p3.n.z * dot(p3.n, r.direction) + r.direction.z));
                return r;
            }
        }
    }

    if(index != 4 && r.idxPlaneCollisioned != 1 && r.idxPlaneCollisioned != 2  && r.idxPlaneCollisioned != 3) {
        // Test on bottom plane
        vec3 cp = computeCollisionPoint(r, texturePlane);
        if(cp.x != -1000.) {
        //Collision with the plane, testing with the triangle
        
            vec3 AB = texturePlane.p2 - texturePlane.p1;
            vec3 AC = texturePlane.p3 - texturePlane.p1;
            vec3 BC = texturePlane.p3 - texturePlane.p2;

            vec3 AM = cp - texturePlane.p1;
            vec3 BM = cp - texturePlane.p2;
            vec3 CM = cp - texturePlane.p3;

            if(dot(normalize(cross(AB,AM)), normalize(cross(AM,AC))) >= 0 
            && dot(normalize(cross(-AB,BM)), normalize(cross(BM,BC))) >= 0 
            && dot(normalize(cross(-AC,CM)), normalize(cross(CM,-BC))) >= 0) {
                r.idxPlaneCollisioned = 4;
                r.start = cp;
                return r;
            }
        }
    }
    return nullRay;
}

void main(void) {

    float depth = texture(Depth, frag.uv).r;
    
    // Screen to 3D coordinates
    vec2  xy = frag.uv * 2.0 -1.0;
    vec4  wPosition =  vec4(xy, depth * 2.0 -1.0, 1.0) * InverseViewProjection;
    vec3  position = vec3(wPosition/wPosition.w);

    Prism prism;
    prism = createPrism();

    vec3 texture = texture(FBOTexture, frag.uv).rgb;
    Color = vec4(texture, 1.0);

    if(PassIndex != 7) {
        // Compute the ray
        Ray r;
        r.start = CameraPosition;
        r.direction = normalize(position - r.start);
        r.idxPlaneCollisioned = -1;

        // Testing the pixel which are not textured (<=> the mirrors)
        if(texelFetch(FBOTexture, ivec2(gl_FragCoord), 0).rgb == vec3(0., 0., 0.)) {
            // Testing once
            r = rayMirror(r, prism.p1, prism.p2, prism.p3, prism.texturePlane);
            if(r.idxPlaneCollisioned != 4) {
                // If one miror has reflected the ray, test it with other mirrors until it touches the bottom or it converges
                if(r.idxPlaneCollisioned != -1) {
                    r = rayMirror(r, prism.p1, prism.p2, prism.p3, prism.texturePlane);
                    // 3D coordinates to screen coordinates
                    vec4 wPp = Projection * View * vec4(r.start, 1.0);
                    vec3 Pp = vec3(wPp/wPp.w);

                    ivec2 ts = textureSize(FBOTexture, 0);
                    //int winX = int((Pp.x +1.5)*FBOTextureWidth/3.); 
                    int winY = int( (Pp.y + sqrt(3.)/2.) * ts.x / sqrt(3.));
                    int winX = int( (Pp.x + sqrt(3.)/2.) * ts.y / sqrt(3.)); // pas normal mais ça marche

                    // Apply its reflection pixel's color
                    Color = vec4(texelFetch(FBOTexture, ivec2(winX, winY), 0).rgb, 1.0);
                }
            }
        }
    }
}
#endif
