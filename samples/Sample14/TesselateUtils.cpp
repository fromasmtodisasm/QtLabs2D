#include "stdafx.h"
#include "TesselateUtils.h"
#include "TeapotData.h"
#include <cmath>
#include <numeric>
#include <limits>

using namespace glm;

namespace
{

constexpr int factorial(int n)
{
	int result = 1;
	for (int i = n; i > 1; i--)
		result *= i;
	return result;
}

constexpr float binomial_coefficient(int i, int n)
{
	return 1.0f * factorial(n) / (factorial(i) * factorial(n - i));
}

constexpr float bernstein_polynomial(int i, int n, float u)
{
	return binomial_coefficient(i, n)
		* static_cast<float>(std::pow(u, i))
		* static_cast<float>(std::pow(1 - u, n - i));
}

vec3 compute_position(vec3 control_points_k[][kTeapotPatchOrder + 1], float u, float v)
{
	vec3 result = { 0.0, 0.0, 0.0 };
	for (int i = 0; i <= static_cast<int>(kTeapotPatchOrder); i++)
	{
		float poly_i = bernstein_polynomial(i, kTeapotPatchOrder, u);
		for (int j = 0; j <= static_cast<int>(kTeapotPatchOrder); j++)
		{
			float poly_j = bernstein_polynomial(j, kTeapotPatchOrder, v);
			result.x += poly_i * poly_j * control_points_k[i][j].x;
			result.y += poly_i * poly_j * control_points_k[i][j].y;
			result.z += poly_i * poly_j * control_points_k[i][j].z;
		}
	}
	return result;
}

void build_control_points_k(size_t p, vec3 control_points_k[][kTeapotPatchOrder + 1])
{
	for (size_t i = 0; i <= kTeapotPatchOrder; i++)
	{
		for (size_t j = 0; j <= kTeapotPatchOrder; j++)
		{
			control_points_k[i][j] = kTeapotControlPoints[kTeapotPatches[p][i][j] - 1];
		}
	}
}

glm::vec3 getPositionOnSphere(float u, float v)
{
	const float radius = 1.f;
	const float latitude = float(M_PI) * (1.f - v); // 𝝅∙(𝟎.𝟓-𝒗)
	const float longitude = float(2.0 * M_PI) * u; // 𝟐𝝅∙𝒖
	const float latitudeRadius = radius * sinf(latitude);

	return {
		cosf(longitude) * latitudeRadius,
		cosf(latitude) * radius,
		sinf(longitude) * latitudeRadius
	};
}

void calculateTriangleStripIndicies(MeshDataP3N3 &data, unsigned columnCount, unsigned rowCount)
{
	data.indicies.clear();
	data.indicies.reserve((columnCount - 1) * rowCount * 2);
	// вычисляем индексы вершин.
	for (unsigned ci = 0; ci < columnCount - 1; ++ci)
	{
		if (ci % 2 == 0)
		{
			for (unsigned ri = 0; ri < rowCount; ++ri)
			{
				unsigned index = ci * rowCount + ri;
				data.indicies.push_back(index + rowCount);
				data.indicies.push_back(index);
			}
		}
		else
		{
			for (unsigned ri = rowCount - 1; ri < rowCount; --ri)
			{
				unsigned index = ci * rowCount + ri;
				data.indicies.push_back(index);
				data.indicies.push_back(index + rowCount);
			}
		}
	}
}

} // namespace

MeshDataP3N3 utils::tesselateSphere(const Material &material, unsigned latitudeDivisions, unsigned longitudeDivisions)
{
	constexpr unsigned kMinPrecision = 4;
	assert((latitudeDivisions >= kMinPrecision) && (longitudeDivisions >= kMinPrecision));

	MeshDataP3N3 data;

	// Для сферы мы используем примитив "полоса треугольников".
	data.primitive = gl::GL_TRIANGLE_STRIP;
	data.material = material;

	data.vertexes.reserve(latitudeDivisions * longitudeDivisions);
	for (unsigned longI = 0; longI < longitudeDivisions; ++longI)
	{
		const float u = float(longI) / float(longitudeDivisions - 1);
		for (unsigned latI = 0; latI < latitudeDivisions; ++latI)
		{
			const float v = float(latI) / float(latitudeDivisions - 1);

			VertexP3N3 vertex;
			vertex.position = getPositionOnSphere(u, v);

			// Нормаль к сфере - это нормализованный вектор радиуса к данной точке
			// Поскольку координаты центра равны 0, координаты вектора радиуса
			// будут равны координатам вершины.
			// Благодаря радиусу, равному 1, нормализация не требуется.
			vertex.normal = vertex.position;

			data.vertexes.push_back(vertex);
		}
	}

	calculateTriangleStripIndicies(data, longitudeDivisions, latitudeDivisions);

	return data;
}

MeshDataP3N3 utils::tesselateTeapot(const Material &material, unsigned latitudeDivisions, unsigned longitudeDivisions)
{
	const size_t pointCount = std::size(kTeapotPatches) * (latitudeDivisions + 1) * (longitudeDivisions + 1);
	const size_t vertexCount = 6 * std::size(kTeapotPatches) * latitudeDivisions * longitudeDivisions;

	std::vector<vec3> points(pointCount);

	// Veticies
	for (size_t p = 0; p < std::size(kTeapotPatches); p++)
	{
		vec3 control_points_k[kTeapotPatchOrder + 1][kTeapotPatchOrder + 1];
		build_control_points_k(p, control_points_k);
		for (size_t ru = 0; ru <= latitudeDivisions - 1; ru++)
		{
			float u = 1.0f * ru / (latitudeDivisions - 1);
			for (size_t rv = 0; rv <= longitudeDivisions - 1; rv++)
			{
				float v = 1.0f * rv / (longitudeDivisions - 1);
				size_t index = p * latitudeDivisions * longitudeDivisions + ru * longitudeDivisions + rv;
				points[index] = compute_position(control_points_k, u, v);
			}
		}
	}


	MeshDataP3N3 result;
	result.primitive = gl::GL_TRIANGLES;
	result.material = material;

	// Каждая вершина будет уникальной из-за различных нормалей, поэтому
	//  индексы будут изменяться последовательно от 0 до vertexCount-1
	result.indicies.resize(vertexCount);
	std::iota(result.indicies.begin(), result.indicies.end(), 0);

	// На основе точек и вычисленных нормалей формируем вершины отдельных треугольников.
	result.vertexes.reserve(vertexCount);
	for (unsigned p = 0; p < std::size(kTeapotPatches); p++)
	{
		for (unsigned ru = 0; ru < latitudeDivisions - 1; ru++)
		{
			for (unsigned rv = 0; rv < longitudeDivisions - 1; rv++)
			{
				// Один фрагмент ABCD делится на два треугольника ABC + CDA
				// На каждой итерации мы добавляем два треугольника.

				// Выделяем точки фрагмента из массива "points".
				const auto getPoint = [&](unsigned ru, unsigned rv) {
					size_t index = p * latitudeDivisions * longitudeDivisions + ru * longitudeDivisions + rv;
					return points.at(index);
				};
				const vec3 pointA = getPoint(ru, rv);
				const vec3 pointB = getPoint(ru, rv + 1);
				const vec3 pointC = getPoint(ru + 1, rv + 1);
				const vec3 pointD = getPoint(ru + 1, rv);

				// Вычисляем нормали к двум треугольникам ABC и CDA.
				const auto getTriangleNormal = [&](vec3 a, vec3 b, vec3 c) {
					vec3 perp = cross(b - a, c - a);
					if (length(perp) < std::numeric_limits<float>::epsilon())
					{
						return vec3(0);
					}
					return normalize(perp);
				};
				const vec3 normalABC = getTriangleNormal(pointA, pointB, pointC);
				const vec3 normalCDA = getTriangleNormal(pointC, pointD, pointA);

				// Формируем треугольник ABC
				result.vertexes.push_back(VertexP3N3{ pointA, normalABC });
				result.vertexes.push_back(VertexP3N3{ pointB, normalABC });
				result.vertexes.push_back(VertexP3N3{ pointC, normalABC });

				// Формируем треугольник CDA
				result.vertexes.push_back(VertexP3N3{ pointC, normalCDA });
				result.vertexes.push_back(VertexP3N3{ pointD, normalCDA });
				result.vertexes.push_back(VertexP3N3{ pointA, normalCDA });
			}
		}
	}

#if !defined(NDEBUG)
	// Проверяем все вершины - значения координат должны действительными числами.
	for (const VertexP3N3& v : result.vertexes)
	{
		assert(std::isfinite(v.position.x));
		assert(std::isfinite(v.position.y));
		assert(std::isfinite(v.position.z));
		assert(std::isfinite(v.normal.x));
		assert(std::isfinite(v.normal.y));
		assert(std::isfinite(v.normal.z));
	}
#endif

	return result;
}
