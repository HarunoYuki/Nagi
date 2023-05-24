/*
	intersect function
*/

float SphereIntersect(vec3 center, float radius, Ray r)
{
    /*
    // 一般推导
	vec3 localOrigin = r.ori - center;
    float a = dot(r.dir, r.dir);
    float b = dot(r.dir, localOrigin) * 2.0;
    float c = dot(localOrigin, localOrigin) - radius * radius;
    float discriminant = b * b - 4 * a * c;
    if(discriminant < 0.0){
        return INF;
    }
    float t1 = (-b - sqrt(discriminant))/(2.0 * a);
    if(t1 > EPSILON){
        return t1;
    }
    float t2 = (-b + sqrt(discriminant))/(2.0 * a);
    if(t2 > EPSILON){
        return t2;
    }
    */
    
    // 假设ray已经归一化, 那么A=1, 基于此化简公式
    vec3 localOrigin = r.ori - center;
    //float A = dot(r.dir, r.dir) = 1.0;
    float halfB = dot(r.dir, localOrigin); // = 0.5*b;
    float C = dot(localOrigin, localOrigin) - radius * radius;
    float discriminant = halfB*halfB - C;
    if(discriminant < 0.0){
        return INF;
    }

    float t1 = -halfB - sqrt(discriminant);
    if(t1 > EPSILON){
        return t1;
    }

    float t2 = -halfB + sqrt(discriminant);
    if(t2 > EPSILON){
        return t2;
    }

	return INF;
}

float RectangleIntersect(vec3 pos, vec3 u, vec3 v, vec4 plane, Ray r)
{
    vec3 n = vec3(plane);
    float t = (plane.w - dot(n, r.ori)) / dot(n, r.dir);

    if(t > EPSILON)
    {
        vec3 p = r.ori + r.dir * t;
        vec3 vi = p - pos;
        float a1 = dot(u, vi);
        if (a1 >= 0.0 && a1 <= 1.0)
        {
            float a2 = dot(v, vi);
            if (a2 >= 0.0 && a2 <= 1.0)
                return t;
        }
        
    }
    
	return INF;
}

float AABBIntersect(vec3 pMin, vec3 pMax, Ray r)
{
    vec3 invDir = 1.0 / r.dir;

    vec3 tNear = (pMin - r.ori) * invDir;
    vec3 tFar = (pMax - r.ori) * invDir;

    vec3 tMin = min(tNear, tFar);
    vec3 tMax = max(tNear, tFar);

    float tEnter = max(tMin.x, max(tMin.y, tMin.z));
    float tExit = min(tMax.x, min(tMax.y, tMax.z));
    // tExit >= tEnter && tEnter < 0.0
    // 那么直接返回tExit，不需要再判断tExit和0的大小关系了
    // 如果tExit > 0.0，有交点，此时tEnter < 0.0 < tExit，t值就是tExit
    // 如果tExit < 0.0，没交点，此时tEnter < tExit < 0.0，返回的t值无用
	return tExit >= tEnter ? (tEnter > 0.0 ? tEnter : tExit) : -1.0;
}