// KRATOS  ___|  |                   |                   |
//       \___ \  __|  __| |   |  __| __| |   |  __| _` | |
//             | |   |    |   | (    |   |   | |   (   | |
//       _____/ \__|_|   \__,_|\___|\__|\__,_|_|  \__,_|_| MECHANICS
//
//  License:         BSD License
//                   license: structural_mechanics_application/license.txt
//
//  Main authors:    Michael Loibl
//					 Tobias Teschemacher
//					 Riccardo Rossi
//
//  Based on work of Tesser and Talledo (University of Padua)

// System includes

// External includes

// Project includes
#include "tc_plastic_damage_3d_law.h"
#include "structural_mechanics_application.h"
#include "structural_mechanics_application_variables.h"

namespace Kratos
{
	double& TCPlasticDamage3DLaw::GetValue(
		const Variable<double>& rThisVariable,
		double& rValue)
	{
		rValue = 0.0;
		//if (rThisVariable == DAMAGE_T)
		//    rValue = m_damage_t;
		//else if (rThisVariable == DAMAGE_C)
		//    rValue = m_damage_c;
		return rValue;
	}

	Vector& TCPlasticDamage3DLaw::GetValue(
		const Variable<Vector>& rThisVariable,
		Vector& rValue)
	{
		//if (rThisVariable == EIGENVALUE_VECTOR)
		//    rValue = m_eigen_values;
		return rValue;
	}

	Matrix& TCPlasticDamage3DLaw::GetValue(
		const Variable<Matrix>& rThisVariable,
		Matrix& rValue)
	{
		//if (rThisVariable == EIGENVECTOR_MATRIX)
		//    rValue = m_eigen_vectors;
		return rValue;
	}

	void TCPlasticDamage3DLaw::SetValue(
			Variable<double>& rVariable,
			const Variable<double>& rValue,
			const ProcessInfo& rCurrentProcessInfo)
	{
		rVariable = rValue;
	}

	void TCPlasticDamage3DLaw::Diagnose()
	{
		m_diagnose = elem2;
	}

	void TCPlasticDamage3DLaw::InitializeMaterial(
		const Properties& rMaterialProperties,
		const GeometryType& rElementGeometry,
		const Vector& rShapeFunctionsValues)
	{
		// diagnostics
		m_diagnose = elem1;
		
		// compressive_strength is negative
		m_elastic_uniaxial_compressive_strength = rMaterialProperties[ELASTIC_UNIAXIAL_STRENGTH_COMPRESSION];
		m_tensile_strength = rMaterialProperties[ELASTIC_UNIAXIAL_STRENGTH_TENSION];
		m_elastic_biaxial_compressive_strength = rMaterialProperties[ELASTIC_BIAXIAL_STRENGTH_COMPRESSION];
		if ((m_elastic_uniaxial_compressive_strength > 0) || (m_elastic_biaxial_compressive_strength > 0))
			{
				std::cout << "Check compressive strengths! They have to be negative." << std::endl;
				std::terminate();
			}
		m_E = rMaterialProperties[YOUNG_MODULUS];
		m_nu = rMaterialProperties[POISSON_RATIO];
		m_Gf = rMaterialProperties[FRACTURE_ENERGY_TENSION];

		CalculateElasticityMatrix(m_D0);
		m_D = m_D0;

		m_elastic_strain = ZeroVector(6);
		m_plastic_strain = ZeroVector(6);

		m_beta = 0;			// method so far implemented neglected plastic strain (ML)

		m_K = sqrt(2.0) * (m_elastic_biaxial_compressive_strength - m_elastic_uniaxial_compressive_strength)/(2 * m_elastic_biaxial_compressive_strength - m_elastic_uniaxial_compressive_strength);
		m_used_equivalent_effective_stress_definition = 2;

		// D+D- Damage Constitutive laws variables
		if (m_used_equivalent_effective_stress_definition == 1)
			{
				m_initial_damage_threshold_compression = sqrt(sqrt(3.0)*(m_K - sqrt(2.0))*m_elastic_uniaxial_compressive_strength / 3);
				m_initial_damage_threshold_tension = sqrt(m_tensile_strength / sqrt(m_E));
			}
		else if (m_used_equivalent_effective_stress_definition == 3)
			{
				m_initial_damage_threshold_compression = sqrt(sqrt(3.0)*(m_K - sqrt(2.0))*m_elastic_uniaxial_compressive_strength / 3);
				m_initial_damage_threshold_tension = sqrt(m_tensile_strength);
			}
		else if (m_used_equivalent_effective_stress_definition == 2)
			{
				m_initial_damage_threshold_compression = sqrt(3.0)*(m_K - sqrt(2.0))*m_elastic_uniaxial_compressive_strength / 3;
				m_initial_damage_threshold_tension = m_tensile_strength;
			}
		m_damage_compression = 0.0;
		m_damage_tension = 0.0;
		// if (rElementGeometry.GetPoint(1).Id()==1)
		// 	KRATOS_WATCH(m_damage_tension);
		m_damage_threshold_compression = m_initial_damage_threshold_compression;
		m_damage_threshold_compression1 = m_damage_threshold_compression;
		m_damage_threshold_tension = m_initial_damage_threshold_tension;
		m_damage_threshold_tension1 = m_damage_threshold_tension;
		// if (rElementGeometry.GetPoint(1).Id()==1)
		// 	KRATOS_WATCH(m_damage_threshold_compression1);
		// if (rElementGeometry.GetPoint(1).Id()==1)
		// 	KRATOS_WATCH(m_damage_threshold_tension1);
		m_compression_parameter_A = rMaterialProperties[COMPRESSION_PARAMETER_A];
		m_compression_parameter_B = rMaterialProperties[COMPRESSION_PARAMETER_B];
		/** be aware: area-function generally not perfectly implemented, returns volume for 3D-elements
		and is not defined in IGA-Application (ML)*/
		double l_c = rElementGeometry.Area();
		m_tension_parameter_A = 1 / ((1 - m_beta)*((m_Gf * m_E / (l_c * m_tensile_strength * m_tensile_strength)) - 0.5));
		m_strain_ref = rMaterialProperties[STRAIN_REFERENCE];
	}

	/***********************************************************************************/
	/***********************************************************************************/
	void TCPlasticDamage3DLaw::CalculateMaterialResponsePK1(ConstitutiveLaw::Parameters& rValues)
	{
		this->CalculateMaterialResponseCauchy(rValues);
	}

	/***********************************************************************************/
	/***********************************************************************************/
	void TCPlasticDamage3DLaw::CalculateMaterialResponsePK2(ConstitutiveLaw::Parameters& rValues)
	{
		this->CalculateMaterialResponseCauchy(rValues);
	}

	/***********************************************************************************/
	/***********************************************************************************/
	void TCPlasticDamage3DLaw::CalculateMaterialResponseKirchhoff(ConstitutiveLaw::Parameters& rValues)
	{
		this->CalculateMaterialResponseCauchy(rValues);
	}

	/***********************************************************************************/
	/***********************************************************************************/
	void TCPlasticDamage3DLaw::CalculateMaterialResponseCauchy(Parameters& rValues)
	{
		const ProcessInfo&  pinfo = rValues.GetProcessInfo();
		const GeometryType& geom = rValues.GetElementGeometry();
		const Properties&   props = rValues.GetMaterialProperties();

		const Vector& rStrainVector = rValues.GetStrainVector();
		Vector&       rStressVector = rValues.GetStressVector();
		Matrix& rConstitutiveLaw = rValues.GetConstitutiveMatrix();

		CalculateMaterialResponseInternal(rStrainVector, rStressVector, rConstitutiveLaw);
	}

	/***********************************************************************************/
	/***********************************************************************************/

	void TCPlasticDamage3DLaw::InitializeMaterialResponsePK1(ConstitutiveLaw::Parameters& rValues)
	{
		// Small deformation so we can call the Cauchy method
		InitializeMaterialResponseCauchy(rValues);
	}

	/***********************************************************************************/
	/***********************************************************************************/

	void TCPlasticDamage3DLaw::InitializeMaterialResponsePK2(ConstitutiveLaw::Parameters& rValues)
	{
		// Small deformation so we can call the Cauchy method
		InitializeMaterialResponseCauchy(rValues);
	}

	/***********************************************************************************/
	/***********************************************************************************/

	void TCPlasticDamage3DLaw::InitializeMaterialResponseCauchy(ConstitutiveLaw::Parameters& rValues)
	{
		// TODO: Add if necessary
	}

	/***********************************************************************************/
	/***********************************************************************************/

	void TCPlasticDamage3DLaw::InitializeMaterialResponseKirchhoff(ConstitutiveLaw::Parameters& rValues)
	{
		// Small deformation so we can call the Cauchy method
		InitializeMaterialResponseCauchy(rValues);
	}

	/***********************************************************************************/
	/***********************************************************************************/
	void TCPlasticDamage3DLaw::FinalizeMaterialResponsePK1(ConstitutiveLaw::Parameters& rValues)
	{
		FinalizeMaterialResponseCauchy(rValues);
	}

	/***********************************************************************************/
	/***********************************************************************************/
	void TCPlasticDamage3DLaw::FinalizeMaterialResponsePK2(ConstitutiveLaw::Parameters& rValues)
	{
		FinalizeMaterialResponseCauchy(rValues);
	}

	/***********************************************************************************/
	/***********************************************************************************/
	void TCPlasticDamage3DLaw::FinalizeMaterialResponseKirchhoff(ConstitutiveLaw::Parameters& rValues)
	{
		FinalizeMaterialResponseCauchy(rValues);
	}

	/***********************************************************************************/
	/***********************************************************************************/
	void TCPlasticDamage3DLaw::FinalizeMaterialResponseCauchy(ConstitutiveLaw::Parameters& rValues)
	{
		const ProcessInfo&  pinfo = rValues.GetProcessInfo();
		const GeometryType& geom = rValues.GetElementGeometry();
		const Properties&   props = rValues.GetMaterialProperties();

		const Vector& rStrainVector = rValues.GetStrainVector();
		Vector&       rStressVector = rValues.GetStressVector();
		Matrix& rConstitutiveLaw = rValues.GetConstitutiveMatrix();

		FinalizeMaterialResponseInternal(rStrainVector, rStressVector, rConstitutiveLaw);
	}

	int TCPlasticDamage3DLaw::Check(
    const Properties& rMaterialProperties,
    const GeometryType& rElementGeometry,
    const ProcessInfo& rCurrentProcessInfo
    )
	{
		KRATOS_CHECK_VARIABLE_KEY(YOUNG_MODULUS);
		KRATOS_ERROR_IF(rMaterialProperties[YOUNG_MODULUS] <= 0.0) << "YOUNG_MODULUS is invalid value " << std::endl;

		KRATOS_CHECK_VARIABLE_KEY(POISSON_RATIO);
		const double& nu = rMaterialProperties[POISSON_RATIO];
		const bool check = static_cast<bool>((nu >0.499 && nu<0.501) || (nu < -0.999 && nu > -1.01));
		KRATOS_ERROR_IF(check) << "POISSON_RATIO is invalid value " << std::endl;

		KRATOS_CHECK_VARIABLE_KEY(DENSITY);
		KRATOS_ERROR_IF(rMaterialProperties[DENSITY] < 0.0) << "DENSITY is invalid value " << std::endl;

		return 0;
	}

	/** this function could be designed as in "elastic_isotropic_3d.cpp"
	with flags checking the respective options whether strain, stress and stiffness matrix
	should be computed here or in the element (ML)*/
	void TCPlasticDamage3DLaw::CalculateMaterialResponseInternal(
		const Vector& rStrainVector,
		Vector& rStressVector,
		Matrix& rConstitutiveLaw)
	{
		
		/* diagnostics */
		if (m_diagnose==elem2)
			KRATOS_WATCH(rStrainVector);
		// static unsigned int counter = 0;
    	// std::cout << ++counter << std::endl;
		
		if (rStressVector.size() != 6)
		{
			rStressVector.resize(6, false);
		}
		// Cauchy stress tensor
		noalias(rStressVector) = prod(m_D, rStrainVector);
		
		// Pass stiffness matrix
		rConstitutiveLaw = m_D;

		// diagnostics
		if (m_diagnose==elem2)
			KRATOS_WATCH(m_D);
	}

	void TCPlasticDamage3DLaw::FinalizeMaterialResponseInternal(
		const Vector& rStrainVector,
		Vector& rStressVector,
		Matrix& rConstitutiveLaw)
	{
		if (rStressVector.size() != 6)
		{
			rStressVector.resize(6, false);
		}

		const double tolerance = (1.0e-14) * m_elastic_uniaxial_compressive_strength;		//move to function "DamageCriterion" if only used once (ML)
		// 1.step: elastic stress tensor
		noalias(rStressVector) = prod(trans(m_D0), (rStrainVector - m_plastic_strain));

		// 2.step: spectral decomposition
		Vector stress_vector_tension = ZeroVector(6);
		Vector stress_vector_compression = ZeroVector(6);
		Vector stress_eigenvalues = ZeroVector(3);
		Matrix p_matrix_tension = ZeroMatrix(6, 6);
		Matrix p_matrix_compression = ZeroMatrix(6, 6);

		SpectralDecomposition(rStressVector,
			stress_vector_tension,
			stress_vector_compression,
			stress_eigenvalues,
			p_matrix_tension,
			p_matrix_compression);

		// 3.step: equivalent effective stress tau
		double equ_eff_stress_compression;
		double equ_eff_stress_tension;

		ComputeTau(stress_eigenvalues,
			equ_eff_stress_compression,
			equ_eff_stress_tension);

		// 4.step: check damage criterion and update damage threshold
		DamageCriterion(equ_eff_stress_compression,
			equ_eff_stress_tension,
			tolerance);

		// 5.step: compute damage variables
		ComputeDamageCompression();

		ComputeDamageTension();

		// 6.step: compute shear retention factors
		ComputeSRF(rStrainVector);

		// 7.step: update stiffness matrix (damaged system)
		m_D = prod(((1 - m_damage_tension) * p_matrix_tension + (1 - m_damage_compression) * p_matrix_compression), m_D0);

		// 8.step: compute Cauchy stress (damaged system)
		rStressVector = (1 - m_damage_tension) * stress_vector_tension + (1 - m_damage_compression) * stress_vector_compression;

		// diagnostics
		if (m_diagnose==elem2)
		{
			KRATOS_WATCH(m_damage_tension);
		}
	}

	void TCPlasticDamage3DLaw::CalculateElasticityMatrix(
    	Matrix& rElasticityMatrix)
	{
    	const double lambda = m_E * m_nu / ((1.0 + m_nu) * (1.0 - 2 * m_nu));
    	const double mu = lambda / (2 * (1 + m_nu));

		if (rElasticityMatrix.size1() != 6 || rElasticityMatrix.size2() != 6)
			rElasticityMatrix.resize(6, 6, false);
		rElasticityMatrix = ZeroMatrix(6,6);

		rElasticityMatrix(0, 0) = 2 * mu + lambda;
		rElasticityMatrix(0, 1) = mu;
		rElasticityMatrix(0, 2) = mu;

		rElasticityMatrix(1, 0) = mu;
		rElasticityMatrix(1, 1) = 2 * mu + lambda;
		rElasticityMatrix(1, 2) = mu;

		rElasticityMatrix(2, 0) = mu;
		rElasticityMatrix(2, 1) = mu;
		rElasticityMatrix(2, 2) = 2 * mu + lambda;

		rElasticityMatrix(3, 3) = mu;

		rElasticityMatrix(4, 4) = mu;

		rElasticityMatrix(5, 5) = mu;
	};

	void TCPlasticDamage3DLaw::SpectralDecomposition(
		const Vector& rStressVector,
		Vector& rStressVectorTension,
		Vector& rStressVectorCompression,
		Vector& rStressEigenvalues,
		Matrix& rPMatrixTension,
		Matrix& rPMatrixCompression)
	{
		Matrix help_matrix = ZeroMatrix(6, 6);

		/** necessary if SpectralDecomposition becomes a static member function
		rStressVectorTension = ZeroVector(6);
		rStressVectorCompression = ZeroVector(6);
		rStressEigenvalues = ZeroVector(3);
		rPMatrixTension = ZeroMatrix(6, 6);
		rPMatrixCompression = ZeroMatrix(6, 6);
		*/

		BoundedMatrix<double, 3, 3> stress_33;
		BoundedMatrix<double, 3, 3> eigenvalue_stress_33;
		BoundedMatrix<double, 3, 3> eigenvector_stress_33;

		stress_33 = MathUtils<double>::StressVectorToTensor(rStressVector);

		bool converged = MathUtils<double>::EigenSystem<3>(stress_33, eigenvector_stress_33, eigenvalue_stress_33);

		for (IndexType i = 0; i < 3; ++i){
			rStressEigenvalues(i) = eigenvalue_stress_33(i, i);
		}

		vector<Vector> p_i(3);
		for (IndexType i = 0; i < 3; ++i) {
			p_i[i] = ZeroVector(3);
			for (IndexType j = 0; j < 3; ++j) {
				p_i[i](j) = eigenvector_stress_33(j, i);
			}
		}

		for (IndexType i = 0; i < 3; ++i) {
			if (eigenvalue_stress_33(i, i) > 0.0) {
				help_matrix  = outer_prod(p_i[i], p_i[i]); // p_i x p_i
				rStressVectorTension += MathUtils<double>::StressTensorToVector(eigenvalue_stress_33(i, i) * help_matrix);
			}
		}
		rStressVectorCompression = rStressVector - rStressVectorTension;

		rPMatrixTension = ZeroMatrix(6, 6);
		for (SizeType i = 0.0; i < 3; ++i)
		{
			if (eigenvalue_stress_33(i, i) > 0.0)
			{
				Vector helpvector = MathUtils<double>::StressTensorToVector(help_matrix);
				rPMatrixTension += outer_prod(helpvector, helpvector);
			}
		}
		Matrix I = IdentityMatrix(6, 6);
		rPMatrixCompression = I - rPMatrixTension;
	};

	void TCPlasticDamage3DLaw::ComputeTau(
		const Vector& rStressEigenvalues,
		double& rEquEffStressCompression,
		double& rEquEffStressTension)
	{
		Vector eigenvalues_compression(3);
		Vector eigenvalues_tension(3);
		for (IndexType i = 0; i < 3; i++)
			eigenvalues_compression(i) = (rStressEigenvalues(i) - fabs(rStressEigenvalues(i)))/2.0;
		for (IndexType i = 0; i < 3; i++)
			eigenvalues_tension(i) = (rStressEigenvalues(i) + fabs(rStressEigenvalues(i)))/2.0;

		double sigoct = (eigenvalues_compression(0) + eigenvalues_compression(1) + eigenvalues_compression(2))/3.0;
		double tauoct = sqrt((eigenvalues_compression(0)-eigenvalues_compression(1)) * (eigenvalues_compression(0) - eigenvalues_compression(1))
		+ (eigenvalues_compression(0) - eigenvalues_compression(2)) * (eigenvalues_compression(0) - eigenvalues_compression(2))
		+ (eigenvalues_compression(1) - eigenvalues_compression(2)) * (eigenvalues_compression(1) - eigenvalues_compression(2)))/3.0;

		if ((m_used_equivalent_effective_stress_definition == 1)  || (m_used_equivalent_effective_stress_definition == 3))
			{
			rEquEffStressCompression = sqrt(3.0) * (m_K * sigoct + tauoct);
			if (rEquEffStressCompression >= 0)
				rEquEffStressCompression = sqrt(rEquEffStressCompression);
			else
				rEquEffStressCompression = 0;
			}
		else if (m_used_equivalent_effective_stress_definition == 2)
			{
			rEquEffStressCompression = sqrt(3.0) * (m_K * sigoct + tauoct);
			}

		double diag = (eigenvalues_tension(0) + eigenvalues_tension(1) + eigenvalues_tension(2)) * m_nu/(-m_E);
		Vector elastic_strain_diag_positive(3);
		for (IndexType i = 0; i < 3; i++)
			{
				elastic_strain_diag_positive(i) = eigenvalues_tension(i) * (1 + m_nu)/m_E + diag;
			}

		rEquEffStressTension = 0.0;
		for (IndexType i = 0; i < 3; i++)
			{
				rEquEffStressTension += elastic_strain_diag_positive(i) * eigenvalues_tension(i);
			}

		if (m_used_equivalent_effective_stress_definition == 1)
			{
			rEquEffStressTension = sqrt(rEquEffStressTension);
			}
		else if (m_used_equivalent_effective_stress_definition == 3)
			{
			rEquEffStressTension = sqrt(sqrt(rEquEffStressTension * m_E));
			}
		else if (m_used_equivalent_effective_stress_definition == 2)
			{
			rEquEffStressTension = sqrt(rEquEffStressTension * m_E);
			}
	};

	void TCPlasticDamage3DLaw::DamageCriterion(
		const double& rEquEffStressCompression,
		const double& rEquEffStressTension,
		const double& rtolerance)
	{
		double g = (rEquEffStressTension/m_damage_threshold_tension)*(rEquEffStressTension/m_damage_threshold_tension)+(rEquEffStressCompression/m_damage_threshold_compression)*(rEquEffStressCompression/m_damage_threshold_compression)-1;
		// diagnostics
		if (m_diagnose==elem2)
			KRATOS_WATCH(g);

		if (g > rtolerance)
			{
			double rhoQ = sqrt(rEquEffStressTension*rEquEffStressTension+rEquEffStressCompression*rEquEffStressCompression);
			double thetaQ;
			if (rEquEffStressCompression > 1e-15)
				{
				thetaQ=atan(rEquEffStressTension/rEquEffStressCompression);
				}
			else
				{
				thetaQ=std::atan(1)*4/2.0;		//atan(1)*4=Pi
				}
			double rhoP = m_damage_threshold_tension*m_damage_threshold_compression*sqrt((rEquEffStressCompression*rEquEffStressCompression+rEquEffStressTension*rEquEffStressTension)/((rEquEffStressCompression*m_damage_threshold_tension)*(rEquEffStressCompression*m_damage_threshold_tension)+(rEquEffStressTension*m_damage_threshold_compression)*(rEquEffStressTension*m_damage_threshold_compression)));
			if (m_damage_threshold_compression >= m_damage_threshold_tension)
				{
				if (rhoP<m_damage_threshold_tension)
					rhoP =m_damage_threshold_tension;
				if (rhoP>m_damage_threshold_compression)
					rhoP = m_damage_threshold_compression;
				}
			else if (m_damage_threshold_compression < m_damage_threshold_tension)
				{
				if (rhoP>m_damage_threshold_tension)
					rhoP =m_damage_threshold_tension;
				if (rhoP<m_damage_threshold_compression)
					rhoP = m_damage_threshold_compression;
				}
			double alfa=rhoQ/rhoP;
			double thetaL = atan((m_damage_threshold_tension*m_damage_threshold_tension)/(m_damage_threshold_compression*m_damage_threshold_compression));
			double rhoL=sqrt((m_damage_threshold_tension*m_damage_threshold_tension*m_damage_threshold_compression*m_damage_threshold_compression)/(m_damage_threshold_compression*m_damage_threshold_compression*sin(thetaL)*sin(thetaL)+m_damage_threshold_tension*m_damage_threshold_tension*cos(thetaL)*cos(thetaL)));
			double alfasn;
			if (((rhoP>rhoL) && (rhoP<=m_damage_threshold_compression)) || ((rhoP>=m_damage_threshold_compression) && (rhoP<rhoL)))
				{
					double alfasp=1+(alfa-1)*(m_damage_threshold_compression-rhoP)/(m_damage_threshold_compression-rhoL);
					m_damage_threshold_tension1=m_damage_threshold_tension*alfasp;
					m_damage_threshold_compression1=sqrt((m_damage_threshold_tension1*m_damage_threshold_tension1*rEquEffStressCompression*rEquEffStressCompression)/(m_damage_threshold_tension1*m_damage_threshold_tension1-rEquEffStressTension*rEquEffStressTension));
				}
			else if (((rhoP>rhoL) && (rhoP<=m_damage_threshold_tension)) || ((rhoP>=m_damage_threshold_tension) && (rhoP<rhoL)))
				{
				alfasn=1+(alfa-1)*(rhoP-m_damage_threshold_tension)/(rhoL-m_damage_threshold_tension);
				m_damage_threshold_compression1=m_damage_threshold_compression*alfasn;
				m_damage_threshold_tension1=sqrt((m_damage_threshold_compression1*m_damage_threshold_compression1*rEquEffStressTension*rEquEffStressTension)/(m_damage_threshold_compression1*m_damage_threshold_compression1-rEquEffStressCompression*rEquEffStressCompression));
				}
			else
				{
				alfasn=1+(alfa-1)*(rhoP-m_damage_threshold_tension)/(rhoL-m_damage_threshold_tension);
				m_damage_threshold_compression1=m_damage_threshold_compression*alfasn;
				m_damage_threshold_tension1=sqrt((m_damage_threshold_compression1*m_damage_threshold_compression1*rEquEffStressTension*rEquEffStressTension)/(m_damage_threshold_compression1*m_damage_threshold_compression1-rEquEffStressCompression*rEquEffStressCompression));
				}
			}
		else
			{
			m_damage_threshold_compression1=m_damage_threshold_compression;
			m_damage_threshold_tension1=m_damage_threshold_tension;
			};
		// KRATOS_WATCH(m_damage_threshold_tension1);
	};

	void TCPlasticDamage3DLaw::ComputeDamageCompression()
	{
		double rDamageCompression;
		if (m_damage_threshold_compression1 < 1e-7)
			{
			// m_damage_compression remains unchanged
			}
		else
			{
				if ((m_used_equivalent_effective_stress_definition == 1)  || (m_used_equivalent_effective_stress_definition == 3))
					rDamageCompression = 1 - m_initial_damage_threshold_compression/m_damage_threshold_compression1 * (1 - m_compression_parameter_A) - m_compression_parameter_A * exp(m_compression_parameter_B * (1 - m_damage_threshold_compression1/m_initial_damage_threshold_compression));
				else if (m_used_equivalent_effective_stress_definition == 2)
					rDamageCompression = 1 - (sqrt(m_initial_damage_threshold_compression))/(sqrt(m_damage_threshold_compression1)) * (1 - m_compression_parameter_A) - m_compression_parameter_A * exp(m_compression_parameter_B * (1 - (sqrt(m_damage_threshold_compression1)/(sqrt(m_initial_damage_threshold_compression)))));
    		    // limiting damage variable
				rDamageCompression = std::min(rDamageCompression, 1.0 - 1e-7);		// maximum not equal to 1.0 since then the stiffness matrix would be equal to 0.0
        		rDamageCompression = std::max(rDamageCompression, 0.0);
				// update damage variable
				m_damage_compression = std::max(m_damage_compression,rDamageCompression);
			}
	}

	void TCPlasticDamage3DLaw::ComputeDamageTension()
	{
		double rDamageTension;
		if (m_damage_threshold_tension1 < 1e-7)
			{
			// m_damage_tension remains unchanged
			}
		else
			{
				if ((m_used_equivalent_effective_stress_definition == 1)  || (m_used_equivalent_effective_stress_definition == 2))
					{
						// KRATOS_WATCH(m_initial_damage_threshold_tension);
						// KRATOS_WATCH(m_damage_threshold_tension1);
						rDamageTension = 1 - m_initial_damage_threshold_tension / m_damage_threshold_tension1 *
						exp(m_tension_parameter_A * (1 - m_damage_threshold_tension1 / m_initial_damage_threshold_tension));
						// KRATOS_WATCH(rDamageTension);
					}
				else if (m_used_equivalent_effective_stress_definition == 3)
					rDamageTension = 1 - (m_initial_damage_threshold_tension * m_initial_damage_threshold_tension)/
						(m_damage_threshold_tension1 * m_damage_threshold_tension1) *
						exp(m_tension_parameter_A * (1 - (m_damage_threshold_tension1 * m_damage_threshold_tension1)/(m_initial_damage_threshold_tension * m_initial_damage_threshold_tension)));
				// limiting damage variable
				rDamageTension = std::min(rDamageTension, 1.0 - 1e-7);
        		rDamageTension = std::max(rDamageTension, 0.0);
				// KRATOS_WATCH(rDamageTension);
				// update damage variable
				m_damage_tension = std::max(m_damage_tension,rDamageTension);
			}
		// KRATOS_WATCH(m_damage_tension);
	}

	void TCPlasticDamage3DLaw::ComputeSRF(const Vector& rStrainVector)
	{
		if (m_strain_ref <= 0.0)
		{
			m_SRF12 = 0.0;
			m_SRF13 = 0.0;
			m_SRF23 = 0.0;
		}
		else
		{
			// evolution law for SRF according to Scotta(2001)
			m_SRF12 = 1-abs(rStrainVector(3))/m_strain_ref;
			m_SRF13 = 1-abs(rStrainVector(4))/m_strain_ref;
			m_SRF23 = 1-abs(rStrainVector(5))/m_strain_ref;
			if (m_SRF12 <= 0.0)
				m_SRF12 = 0.0;
			if (m_SRF13 <= 0.0)
				m_SRF13 = 0.0;
			if (m_SRF23 <= 0.0)
				m_SRF23 = 0.0;
		}
	}
} // namespace Kratos