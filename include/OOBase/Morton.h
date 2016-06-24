///////////////////////////////////////////////////////////////////////////////////
//
// Copyright (C) 2015 Rick Taylor
//
// This file is part of OOBase, the Omega Online Base library.
//
// OOBase is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// OOBase is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with OOBase.  If not, see <http://www.gnu.org/licenses/>.
//
///////////////////////////////////////////////////////////////////////////////////

#ifndef OOBASE_INCLUDE_OOBASE_MORTON_H_
#define OOBASE_INCLUDE_OOBASE_MORTON_H_

namespace OOBase
{
	namespace detail
	{
		template <typename T, size_t S>
		struct MortonMAX
		{
			static const T value = (MortonMAX<T,S/2>::value << MortonMAX<T,S/2>::offset) | MortonMAX<T,S/2>::value;
			static const unsigned int offset = MortonMAX<T,S/2>::offset * 2;
		};

		template <typename T>
		struct MortonMAX<T,1>
		{
			static const T value = 0111; // <- Octal!!
			static const unsigned int offset = 9;
		};

		template <typename T, size_t I>
		struct MortonValue
		{
			// Use a little trick to calculate next morton key
			// Morton(x+1) = (Morton(x) - MAXMORTONKEY) & MAXMORTONKEY
			static const T valueX = (MortonValue<T,I-1>::valueX - MortonMAX<T,sizeof(T)>::value) & MortonMAX<T,sizeof(T)>::value;
			static const T valueY = T(valueX << 1);
			static const T valueZ = T(valueX << 2);
		};

		template <typename T>
		struct MortonValue<T,0>
		{
			static const T valueX = 0;
			static const T valueY = 0;
			static const T valueZ = 0;
		};

		template <typename T>
		struct MortonLUT
		{
			static const T x[256];
			static const T y[256];
			static const T z[256];
		};

		template <typename T>
		const T MortonLUT<T>::x[256] =
		{
			MortonValue<T,0>::valueX,MortonValue<T,1>::valueX,MortonValue<T,2>::valueX,MortonValue<T,3>::valueX,
			MortonValue<T,4>::valueX,MortonValue<T,5>::valueX,MortonValue<T,6>::valueX,MortonValue<T,7>::valueX,
			MortonValue<T,8>::valueX,MortonValue<T,9>::valueX,MortonValue<T,10>::valueX,MortonValue<T,11>::valueX,
			MortonValue<T,12>::valueX,MortonValue<T,13>::valueX,MortonValue<T,14>::valueX,MortonValue<T,15>::valueX,
			MortonValue<T,16>::valueX,MortonValue<T,17>::valueX,MortonValue<T,18>::valueX,MortonValue<T,19>::valueX,
			MortonValue<T,20>::valueX,MortonValue<T,21>::valueX,MortonValue<T,22>::valueX,MortonValue<T,23>::valueX,
			MortonValue<T,24>::valueX,MortonValue<T,25>::valueX,MortonValue<T,26>::valueX,MortonValue<T,27>::valueX,
			MortonValue<T,28>::valueX,MortonValue<T,29>::valueX,MortonValue<T,30>::valueX,MortonValue<T,31>::valueX,
			MortonValue<T,32>::valueX,MortonValue<T,33>::valueX,MortonValue<T,34>::valueX,MortonValue<T,35>::valueX,
			MortonValue<T,36>::valueX,MortonValue<T,37>::valueX,MortonValue<T,38>::valueX,MortonValue<T,39>::valueX,
			MortonValue<T,40>::valueX,MortonValue<T,41>::valueX,MortonValue<T,42>::valueX,MortonValue<T,43>::valueX,
			MortonValue<T,44>::valueX,MortonValue<T,45>::valueX,MortonValue<T,46>::valueX,MortonValue<T,47>::valueX,
			MortonValue<T,48>::valueX,MortonValue<T,49>::valueX,MortonValue<T,50>::valueX,MortonValue<T,51>::valueX,
			MortonValue<T,52>::valueX,MortonValue<T,53>::valueX,MortonValue<T,54>::valueX,MortonValue<T,55>::valueX,
			MortonValue<T,56>::valueX,MortonValue<T,57>::valueX,MortonValue<T,58>::valueX,MortonValue<T,59>::valueX,
			MortonValue<T,60>::valueX,MortonValue<T,61>::valueX,MortonValue<T,62>::valueX,MortonValue<T,63>::valueX,
			MortonValue<T,64>::valueX,MortonValue<T,65>::valueX,MortonValue<T,66>::valueX,MortonValue<T,67>::valueX,
			MortonValue<T,68>::valueX,MortonValue<T,69>::valueX,MortonValue<T,70>::valueX,MortonValue<T,71>::valueX,
			MortonValue<T,72>::valueX,MortonValue<T,73>::valueX,MortonValue<T,74>::valueX,MortonValue<T,75>::valueX,
			MortonValue<T,76>::valueX,MortonValue<T,77>::valueX,MortonValue<T,78>::valueX,MortonValue<T,79>::valueX,
			MortonValue<T,80>::valueX,MortonValue<T,81>::valueX,MortonValue<T,82>::valueX,MortonValue<T,83>::valueX,
			MortonValue<T,84>::valueX,MortonValue<T,85>::valueX,MortonValue<T,86>::valueX,MortonValue<T,87>::valueX,
			MortonValue<T,88>::valueX,MortonValue<T,89>::valueX,MortonValue<T,90>::valueX,MortonValue<T,91>::valueX,
			MortonValue<T,92>::valueX,MortonValue<T,93>::valueX,MortonValue<T,94>::valueX,MortonValue<T,95>::valueX,
			MortonValue<T,96>::valueX,MortonValue<T,97>::valueX,MortonValue<T,98>::valueX,MortonValue<T,99>::valueX,
			MortonValue<T,100>::valueX,MortonValue<T,101>::valueX,MortonValue<T,102>::valueX,MortonValue<T,103>::valueX,
			MortonValue<T,104>::valueX,MortonValue<T,105>::valueX,MortonValue<T,106>::valueX,MortonValue<T,107>::valueX,
			MortonValue<T,108>::valueX,MortonValue<T,109>::valueX,MortonValue<T,110>::valueX,MortonValue<T,111>::valueX,
			MortonValue<T,112>::valueX,MortonValue<T,113>::valueX,MortonValue<T,114>::valueX,MortonValue<T,115>::valueX,
			MortonValue<T,116>::valueX,MortonValue<T,117>::valueX,MortonValue<T,118>::valueX,MortonValue<T,119>::valueX,
			MortonValue<T,120>::valueX,MortonValue<T,121>::valueX,MortonValue<T,122>::valueX,MortonValue<T,123>::valueX,
			MortonValue<T,124>::valueX,MortonValue<T,125>::valueX,MortonValue<T,126>::valueX,MortonValue<T,127>::valueX,
			MortonValue<T,128>::valueX,MortonValue<T,129>::valueX,MortonValue<T,130>::valueX,MortonValue<T,131>::valueX,
			MortonValue<T,132>::valueX,MortonValue<T,133>::valueX,MortonValue<T,134>::valueX,MortonValue<T,135>::valueX,
			MortonValue<T,136>::valueX,MortonValue<T,137>::valueX,MortonValue<T,138>::valueX,MortonValue<T,139>::valueX,
			MortonValue<T,140>::valueX,MortonValue<T,141>::valueX,MortonValue<T,142>::valueX,MortonValue<T,143>::valueX,
			MortonValue<T,144>::valueX,MortonValue<T,145>::valueX,MortonValue<T,146>::valueX,MortonValue<T,147>::valueX,
			MortonValue<T,148>::valueX,MortonValue<T,149>::valueX,MortonValue<T,150>::valueX,MortonValue<T,151>::valueX,
			MortonValue<T,152>::valueX,MortonValue<T,153>::valueX,MortonValue<T,154>::valueX,MortonValue<T,155>::valueX,
			MortonValue<T,156>::valueX,MortonValue<T,157>::valueX,MortonValue<T,158>::valueX,MortonValue<T,159>::valueX,
			MortonValue<T,160>::valueX,MortonValue<T,161>::valueX,MortonValue<T,162>::valueX,MortonValue<T,163>::valueX,
			MortonValue<T,164>::valueX,MortonValue<T,165>::valueX,MortonValue<T,166>::valueX,MortonValue<T,167>::valueX,
			MortonValue<T,168>::valueX,MortonValue<T,169>::valueX,MortonValue<T,170>::valueX,MortonValue<T,171>::valueX,
			MortonValue<T,172>::valueX,MortonValue<T,173>::valueX,MortonValue<T,174>::valueX,MortonValue<T,175>::valueX,
			MortonValue<T,176>::valueX,MortonValue<T,177>::valueX,MortonValue<T,178>::valueX,MortonValue<T,179>::valueX,
			MortonValue<T,180>::valueX,MortonValue<T,181>::valueX,MortonValue<T,182>::valueX,MortonValue<T,183>::valueX,
			MortonValue<T,184>::valueX,MortonValue<T,185>::valueX,MortonValue<T,186>::valueX,MortonValue<T,187>::valueX,
			MortonValue<T,188>::valueX,MortonValue<T,189>::valueX,MortonValue<T,190>::valueX,MortonValue<T,191>::valueX,
			MortonValue<T,192>::valueX,MortonValue<T,193>::valueX,MortonValue<T,194>::valueX,MortonValue<T,195>::valueX,
			MortonValue<T,196>::valueX,MortonValue<T,197>::valueX,MortonValue<T,198>::valueX,MortonValue<T,199>::valueX,
			MortonValue<T,200>::valueX,MortonValue<T,201>::valueX,MortonValue<T,202>::valueX,MortonValue<T,203>::valueX,
			MortonValue<T,204>::valueX,MortonValue<T,205>::valueX,MortonValue<T,206>::valueX,MortonValue<T,207>::valueX,
			MortonValue<T,208>::valueX,MortonValue<T,209>::valueX,MortonValue<T,210>::valueX,MortonValue<T,211>::valueX,
			MortonValue<T,212>::valueX,MortonValue<T,213>::valueX,MortonValue<T,214>::valueX,MortonValue<T,215>::valueX,
			MortonValue<T,216>::valueX,MortonValue<T,217>::valueX,MortonValue<T,218>::valueX,MortonValue<T,219>::valueX,
			MortonValue<T,220>::valueX,MortonValue<T,221>::valueX,MortonValue<T,222>::valueX,MortonValue<T,223>::valueX,
			MortonValue<T,224>::valueX,MortonValue<T,225>::valueX,MortonValue<T,226>::valueX,MortonValue<T,227>::valueX,
			MortonValue<T,228>::valueX,MortonValue<T,229>::valueX,MortonValue<T,230>::valueX,MortonValue<T,231>::valueX,
			MortonValue<T,232>::valueX,MortonValue<T,233>::valueX,MortonValue<T,234>::valueX,MortonValue<T,235>::valueX,
			MortonValue<T,236>::valueX,MortonValue<T,237>::valueX,MortonValue<T,238>::valueX,MortonValue<T,239>::valueX,
			MortonValue<T,240>::valueX,MortonValue<T,241>::valueX,MortonValue<T,242>::valueX,MortonValue<T,243>::valueX,
			MortonValue<T,244>::valueX,MortonValue<T,245>::valueX,MortonValue<T,246>::valueX,MortonValue<T,247>::valueX,
			MortonValue<T,248>::valueX,MortonValue<T,249>::valueX,MortonValue<T,250>::valueX,MortonValue<T,251>::valueX,
			MortonValue<T,252>::valueX,MortonValue<T,253>::valueX,MortonValue<T,254>::valueX,MortonValue<T,255>::valueX
		};

		template <typename T>
		const T MortonLUT<T>::y[256] =
		{
			MortonValue<T,0>::valueY,MortonValue<T,1>::valueY,MortonValue<T,2>::valueY,MortonValue<T,3>::valueY,
			MortonValue<T,4>::valueY,MortonValue<T,5>::valueY,MortonValue<T,6>::valueY,MortonValue<T,7>::valueY,
			MortonValue<T,8>::valueY,MortonValue<T,9>::valueY,MortonValue<T,10>::valueY,MortonValue<T,11>::valueY,
			MortonValue<T,12>::valueY,MortonValue<T,13>::valueY,MortonValue<T,14>::valueY,MortonValue<T,15>::valueY,
			MortonValue<T,16>::valueY,MortonValue<T,17>::valueY,MortonValue<T,18>::valueY,MortonValue<T,19>::valueY,
			MortonValue<T,20>::valueY,MortonValue<T,21>::valueY,MortonValue<T,22>::valueY,MortonValue<T,23>::valueY,
			MortonValue<T,24>::valueY,MortonValue<T,25>::valueY,MortonValue<T,26>::valueY,MortonValue<T,27>::valueY,
			MortonValue<T,28>::valueY,MortonValue<T,29>::valueY,MortonValue<T,30>::valueY,MortonValue<T,31>::valueY,
			MortonValue<T,32>::valueY,MortonValue<T,33>::valueY,MortonValue<T,34>::valueY,MortonValue<T,35>::valueY,
			MortonValue<T,36>::valueY,MortonValue<T,37>::valueY,MortonValue<T,38>::valueY,MortonValue<T,39>::valueY,
			MortonValue<T,40>::valueY,MortonValue<T,41>::valueY,MortonValue<T,42>::valueY,MortonValue<T,43>::valueY,
			MortonValue<T,44>::valueY,MortonValue<T,45>::valueY,MortonValue<T,46>::valueY,MortonValue<T,47>::valueY,
			MortonValue<T,48>::valueY,MortonValue<T,49>::valueY,MortonValue<T,50>::valueY,MortonValue<T,51>::valueY,
			MortonValue<T,52>::valueY,MortonValue<T,53>::valueY,MortonValue<T,54>::valueY,MortonValue<T,55>::valueY,
			MortonValue<T,56>::valueY,MortonValue<T,57>::valueY,MortonValue<T,58>::valueY,MortonValue<T,59>::valueY,
			MortonValue<T,60>::valueY,MortonValue<T,61>::valueY,MortonValue<T,62>::valueY,MortonValue<T,63>::valueY,
			MortonValue<T,64>::valueY,MortonValue<T,65>::valueY,MortonValue<T,66>::valueY,MortonValue<T,67>::valueY,
			MortonValue<T,68>::valueY,MortonValue<T,69>::valueY,MortonValue<T,70>::valueY,MortonValue<T,71>::valueY,
			MortonValue<T,72>::valueY,MortonValue<T,73>::valueY,MortonValue<T,74>::valueY,MortonValue<T,75>::valueY,
			MortonValue<T,76>::valueY,MortonValue<T,77>::valueY,MortonValue<T,78>::valueY,MortonValue<T,79>::valueY,
			MortonValue<T,80>::valueY,MortonValue<T,81>::valueY,MortonValue<T,82>::valueY,MortonValue<T,83>::valueY,
			MortonValue<T,84>::valueY,MortonValue<T,85>::valueY,MortonValue<T,86>::valueY,MortonValue<T,87>::valueY,
			MortonValue<T,88>::valueY,MortonValue<T,89>::valueY,MortonValue<T,90>::valueY,MortonValue<T,91>::valueY,
			MortonValue<T,92>::valueY,MortonValue<T,93>::valueY,MortonValue<T,94>::valueY,MortonValue<T,95>::valueY,
			MortonValue<T,96>::valueY,MortonValue<T,97>::valueY,MortonValue<T,98>::valueY,MortonValue<T,99>::valueY,
			MortonValue<T,100>::valueY,MortonValue<T,101>::valueY,MortonValue<T,102>::valueY,MortonValue<T,103>::valueY,
			MortonValue<T,104>::valueY,MortonValue<T,105>::valueY,MortonValue<T,106>::valueY,MortonValue<T,107>::valueY,
			MortonValue<T,108>::valueY,MortonValue<T,109>::valueY,MortonValue<T,110>::valueY,MortonValue<T,111>::valueY,
			MortonValue<T,112>::valueY,MortonValue<T,113>::valueY,MortonValue<T,114>::valueY,MortonValue<T,115>::valueY,
			MortonValue<T,116>::valueY,MortonValue<T,117>::valueY,MortonValue<T,118>::valueY,MortonValue<T,119>::valueY,
			MortonValue<T,120>::valueY,MortonValue<T,121>::valueY,MortonValue<T,122>::valueY,MortonValue<T,123>::valueY,
			MortonValue<T,124>::valueY,MortonValue<T,125>::valueY,MortonValue<T,126>::valueY,MortonValue<T,127>::valueY,
			MortonValue<T,128>::valueY,MortonValue<T,129>::valueY,MortonValue<T,130>::valueY,MortonValue<T,131>::valueY,
			MortonValue<T,132>::valueY,MortonValue<T,133>::valueY,MortonValue<T,134>::valueY,MortonValue<T,135>::valueY,
			MortonValue<T,136>::valueY,MortonValue<T,137>::valueY,MortonValue<T,138>::valueY,MortonValue<T,139>::valueY,
			MortonValue<T,140>::valueY,MortonValue<T,141>::valueY,MortonValue<T,142>::valueY,MortonValue<T,143>::valueY,
			MortonValue<T,144>::valueY,MortonValue<T,145>::valueY,MortonValue<T,146>::valueY,MortonValue<T,147>::valueY,
			MortonValue<T,148>::valueY,MortonValue<T,149>::valueY,MortonValue<T,150>::valueY,MortonValue<T,151>::valueY,
			MortonValue<T,152>::valueY,MortonValue<T,153>::valueY,MortonValue<T,154>::valueY,MortonValue<T,155>::valueY,
			MortonValue<T,156>::valueY,MortonValue<T,157>::valueY,MortonValue<T,158>::valueY,MortonValue<T,159>::valueY,
			MortonValue<T,160>::valueY,MortonValue<T,161>::valueY,MortonValue<T,162>::valueY,MortonValue<T,163>::valueY,
			MortonValue<T,164>::valueY,MortonValue<T,165>::valueY,MortonValue<T,166>::valueY,MortonValue<T,167>::valueY,
			MortonValue<T,168>::valueY,MortonValue<T,169>::valueY,MortonValue<T,170>::valueY,MortonValue<T,171>::valueY,
			MortonValue<T,172>::valueY,MortonValue<T,173>::valueY,MortonValue<T,174>::valueY,MortonValue<T,175>::valueY,
			MortonValue<T,176>::valueY,MortonValue<T,177>::valueY,MortonValue<T,178>::valueY,MortonValue<T,179>::valueY,
			MortonValue<T,180>::valueY,MortonValue<T,181>::valueY,MortonValue<T,182>::valueY,MortonValue<T,183>::valueY,
			MortonValue<T,184>::valueY,MortonValue<T,185>::valueY,MortonValue<T,186>::valueY,MortonValue<T,187>::valueY,
			MortonValue<T,188>::valueY,MortonValue<T,189>::valueY,MortonValue<T,190>::valueY,MortonValue<T,191>::valueY,
			MortonValue<T,192>::valueY,MortonValue<T,193>::valueY,MortonValue<T,194>::valueY,MortonValue<T,195>::valueY,
			MortonValue<T,196>::valueY,MortonValue<T,197>::valueY,MortonValue<T,198>::valueY,MortonValue<T,199>::valueY,
			MortonValue<T,200>::valueY,MortonValue<T,201>::valueY,MortonValue<T,202>::valueY,MortonValue<T,203>::valueY,
			MortonValue<T,204>::valueY,MortonValue<T,205>::valueY,MortonValue<T,206>::valueY,MortonValue<T,207>::valueY,
			MortonValue<T,208>::valueY,MortonValue<T,209>::valueY,MortonValue<T,210>::valueY,MortonValue<T,211>::valueY,
			MortonValue<T,212>::valueY,MortonValue<T,213>::valueY,MortonValue<T,214>::valueY,MortonValue<T,215>::valueY,
			MortonValue<T,216>::valueY,MortonValue<T,217>::valueY,MortonValue<T,218>::valueY,MortonValue<T,219>::valueY,
			MortonValue<T,220>::valueY,MortonValue<T,221>::valueY,MortonValue<T,222>::valueY,MortonValue<T,223>::valueY,
			MortonValue<T,224>::valueY,MortonValue<T,225>::valueY,MortonValue<T,226>::valueY,MortonValue<T,227>::valueY,
			MortonValue<T,228>::valueY,MortonValue<T,229>::valueY,MortonValue<T,230>::valueY,MortonValue<T,231>::valueY,
			MortonValue<T,232>::valueY,MortonValue<T,233>::valueY,MortonValue<T,234>::valueY,MortonValue<T,235>::valueY,
			MortonValue<T,236>::valueY,MortonValue<T,237>::valueY,MortonValue<T,238>::valueY,MortonValue<T,239>::valueY,
			MortonValue<T,240>::valueY,MortonValue<T,241>::valueY,MortonValue<T,242>::valueY,MortonValue<T,243>::valueY,
			MortonValue<T,244>::valueY,MortonValue<T,245>::valueY,MortonValue<T,246>::valueY,MortonValue<T,247>::valueY,
			MortonValue<T,248>::valueY,MortonValue<T,249>::valueY,MortonValue<T,250>::valueY,MortonValue<T,251>::valueY,
			MortonValue<T,252>::valueY,MortonValue<T,253>::valueY,MortonValue<T,254>::valueY,MortonValue<T,255>::valueY
		};

		template <typename T>
		const T MortonLUT<T>::z[256] =
		{
			MortonValue<T,0>::valueZ,MortonValue<T,1>::valueZ,MortonValue<T,2>::valueZ,MortonValue<T,3>::valueZ,
			MortonValue<T,4>::valueZ,MortonValue<T,5>::valueZ,MortonValue<T,6>::valueZ,MortonValue<T,7>::valueZ,
			MortonValue<T,8>::valueZ,MortonValue<T,9>::valueZ,MortonValue<T,10>::valueZ,MortonValue<T,11>::valueZ,
			MortonValue<T,12>::valueZ,MortonValue<T,13>::valueZ,MortonValue<T,14>::valueZ,MortonValue<T,15>::valueZ,
			MortonValue<T,16>::valueZ,MortonValue<T,17>::valueZ,MortonValue<T,18>::valueZ,MortonValue<T,19>::valueZ,
			MortonValue<T,20>::valueZ,MortonValue<T,21>::valueZ,MortonValue<T,22>::valueZ,MortonValue<T,23>::valueZ,
			MortonValue<T,24>::valueZ,MortonValue<T,25>::valueZ,MortonValue<T,26>::valueZ,MortonValue<T,27>::valueZ,
			MortonValue<T,28>::valueZ,MortonValue<T,29>::valueZ,MortonValue<T,30>::valueZ,MortonValue<T,31>::valueZ,
			MortonValue<T,32>::valueZ,MortonValue<T,33>::valueZ,MortonValue<T,34>::valueZ,MortonValue<T,35>::valueZ,
			MortonValue<T,36>::valueZ,MortonValue<T,37>::valueZ,MortonValue<T,38>::valueZ,MortonValue<T,39>::valueZ,
			MortonValue<T,40>::valueZ,MortonValue<T,41>::valueZ,MortonValue<T,42>::valueZ,MortonValue<T,43>::valueZ,
			MortonValue<T,44>::valueZ,MortonValue<T,45>::valueZ,MortonValue<T,46>::valueZ,MortonValue<T,47>::valueZ,
			MortonValue<T,48>::valueZ,MortonValue<T,49>::valueZ,MortonValue<T,50>::valueZ,MortonValue<T,51>::valueZ,
			MortonValue<T,52>::valueZ,MortonValue<T,53>::valueZ,MortonValue<T,54>::valueZ,MortonValue<T,55>::valueZ,
			MortonValue<T,56>::valueZ,MortonValue<T,57>::valueZ,MortonValue<T,58>::valueZ,MortonValue<T,59>::valueZ,
			MortonValue<T,60>::valueZ,MortonValue<T,61>::valueZ,MortonValue<T,62>::valueZ,MortonValue<T,63>::valueZ,
			MortonValue<T,64>::valueZ,MortonValue<T,65>::valueZ,MortonValue<T,66>::valueZ,MortonValue<T,67>::valueZ,
			MortonValue<T,68>::valueZ,MortonValue<T,69>::valueZ,MortonValue<T,70>::valueZ,MortonValue<T,71>::valueZ,
			MortonValue<T,72>::valueZ,MortonValue<T,73>::valueZ,MortonValue<T,74>::valueZ,MortonValue<T,75>::valueZ,
			MortonValue<T,76>::valueZ,MortonValue<T,77>::valueZ,MortonValue<T,78>::valueZ,MortonValue<T,79>::valueZ,
			MortonValue<T,80>::valueZ,MortonValue<T,81>::valueZ,MortonValue<T,82>::valueZ,MortonValue<T,83>::valueZ,
			MortonValue<T,84>::valueZ,MortonValue<T,85>::valueZ,MortonValue<T,86>::valueZ,MortonValue<T,87>::valueZ,
			MortonValue<T,88>::valueZ,MortonValue<T,89>::valueZ,MortonValue<T,90>::valueZ,MortonValue<T,91>::valueZ,
			MortonValue<T,92>::valueZ,MortonValue<T,93>::valueZ,MortonValue<T,94>::valueZ,MortonValue<T,95>::valueZ,
			MortonValue<T,96>::valueZ,MortonValue<T,97>::valueZ,MortonValue<T,98>::valueZ,MortonValue<T,99>::valueZ,
			MortonValue<T,100>::valueZ,MortonValue<T,101>::valueZ,MortonValue<T,102>::valueZ,MortonValue<T,103>::valueZ,
			MortonValue<T,104>::valueZ,MortonValue<T,105>::valueZ,MortonValue<T,106>::valueZ,MortonValue<T,107>::valueZ,
			MortonValue<T,108>::valueZ,MortonValue<T,109>::valueZ,MortonValue<T,110>::valueZ,MortonValue<T,111>::valueZ,
			MortonValue<T,112>::valueZ,MortonValue<T,113>::valueZ,MortonValue<T,114>::valueZ,MortonValue<T,115>::valueZ,
			MortonValue<T,116>::valueZ,MortonValue<T,117>::valueZ,MortonValue<T,118>::valueZ,MortonValue<T,119>::valueZ,
			MortonValue<T,120>::valueZ,MortonValue<T,121>::valueZ,MortonValue<T,122>::valueZ,MortonValue<T,123>::valueZ,
			MortonValue<T,124>::valueZ,MortonValue<T,125>::valueZ,MortonValue<T,126>::valueZ,MortonValue<T,127>::valueZ,
			MortonValue<T,128>::valueZ,MortonValue<T,129>::valueZ,MortonValue<T,130>::valueZ,MortonValue<T,131>::valueZ,
			MortonValue<T,132>::valueZ,MortonValue<T,133>::valueZ,MortonValue<T,134>::valueZ,MortonValue<T,135>::valueZ,
			MortonValue<T,136>::valueZ,MortonValue<T,137>::valueZ,MortonValue<T,138>::valueZ,MortonValue<T,139>::valueZ,
			MortonValue<T,140>::valueZ,MortonValue<T,141>::valueZ,MortonValue<T,142>::valueZ,MortonValue<T,143>::valueZ,
			MortonValue<T,144>::valueZ,MortonValue<T,145>::valueZ,MortonValue<T,146>::valueZ,MortonValue<T,147>::valueZ,
			MortonValue<T,148>::valueZ,MortonValue<T,149>::valueZ,MortonValue<T,150>::valueZ,MortonValue<T,151>::valueZ,
			MortonValue<T,152>::valueZ,MortonValue<T,153>::valueZ,MortonValue<T,154>::valueZ,MortonValue<T,155>::valueZ,
			MortonValue<T,156>::valueZ,MortonValue<T,157>::valueZ,MortonValue<T,158>::valueZ,MortonValue<T,159>::valueZ,
			MortonValue<T,160>::valueZ,MortonValue<T,161>::valueZ,MortonValue<T,162>::valueZ,MortonValue<T,163>::valueZ,
			MortonValue<T,164>::valueZ,MortonValue<T,165>::valueZ,MortonValue<T,166>::valueZ,MortonValue<T,167>::valueZ,
			MortonValue<T,168>::valueZ,MortonValue<T,169>::valueZ,MortonValue<T,170>::valueZ,MortonValue<T,171>::valueZ,
			MortonValue<T,172>::valueZ,MortonValue<T,173>::valueZ,MortonValue<T,174>::valueZ,MortonValue<T,175>::valueZ,
			MortonValue<T,176>::valueZ,MortonValue<T,177>::valueZ,MortonValue<T,178>::valueZ,MortonValue<T,179>::valueZ,
			MortonValue<T,180>::valueZ,MortonValue<T,181>::valueZ,MortonValue<T,182>::valueZ,MortonValue<T,183>::valueZ,
			MortonValue<T,184>::valueZ,MortonValue<T,185>::valueZ,MortonValue<T,186>::valueZ,MortonValue<T,187>::valueZ,
			MortonValue<T,188>::valueZ,MortonValue<T,189>::valueZ,MortonValue<T,190>::valueZ,MortonValue<T,191>::valueZ,
			MortonValue<T,192>::valueZ,MortonValue<T,193>::valueZ,MortonValue<T,194>::valueZ,MortonValue<T,195>::valueZ,
			MortonValue<T,196>::valueZ,MortonValue<T,197>::valueZ,MortonValue<T,198>::valueZ,MortonValue<T,199>::valueZ,
			MortonValue<T,200>::valueZ,MortonValue<T,201>::valueZ,MortonValue<T,202>::valueZ,MortonValue<T,203>::valueZ,
			MortonValue<T,204>::valueZ,MortonValue<T,205>::valueZ,MortonValue<T,206>::valueZ,MortonValue<T,207>::valueZ,
			MortonValue<T,208>::valueZ,MortonValue<T,209>::valueZ,MortonValue<T,210>::valueZ,MortonValue<T,211>::valueZ,
			MortonValue<T,212>::valueZ,MortonValue<T,213>::valueZ,MortonValue<T,214>::valueZ,MortonValue<T,215>::valueZ,
			MortonValue<T,216>::valueZ,MortonValue<T,217>::valueZ,MortonValue<T,218>::valueZ,MortonValue<T,219>::valueZ,
			MortonValue<T,220>::valueZ,MortonValue<T,221>::valueZ,MortonValue<T,222>::valueZ,MortonValue<T,223>::valueZ,
			MortonValue<T,224>::valueZ,MortonValue<T,225>::valueZ,MortonValue<T,226>::valueZ,MortonValue<T,227>::valueZ,
			MortonValue<T,228>::valueZ,MortonValue<T,229>::valueZ,MortonValue<T,230>::valueZ,MortonValue<T,231>::valueZ,
			MortonValue<T,232>::valueZ,MortonValue<T,233>::valueZ,MortonValue<T,234>::valueZ,MortonValue<T,235>::valueZ,
			MortonValue<T,236>::valueZ,MortonValue<T,237>::valueZ,MortonValue<T,238>::valueZ,MortonValue<T,239>::valueZ,
			MortonValue<T,240>::valueZ,MortonValue<T,241>::valueZ,MortonValue<T,242>::valueZ,MortonValue<T,243>::valueZ,
			MortonValue<T,244>::valueZ,MortonValue<T,245>::valueZ,MortonValue<T,246>::valueZ,MortonValue<T,247>::valueZ,
			MortonValue<T,248>::valueZ,MortonValue<T,249>::valueZ,MortonValue<T,250>::valueZ,MortonValue<T,251>::valueZ,
			MortonValue<T,252>::valueZ,MortonValue<T,253>::valueZ,MortonValue<T,254>::valueZ,MortonValue<T,255>::valueZ
		};
	}

	inline uint64_t MortonEncode64(uint32_t x, uint32_t y, uint32_t z)
	{
		uint64_t result = detail::MortonLUT<uint64_t>::x[(x >> 16) & 0x3F] | detail::MortonLUT<uint64_t>::y[(y >> 16) & 0x1F] | detail::MortonLUT<uint64_t>::z[(z >> 16) & 0x1F];
		result <<= 48;
		result |= detail::MortonLUT<uint64_t>::x[(x >> 8) & 0xFF] | detail::MortonLUT<uint64_t>::y[(y >> 8) & 0xFF] | detail::MortonLUT<uint64_t>::z[(z >> 8) & 0xFF];
		result <<= 24;
		result |= detail::MortonLUT<uint64_t>::x[x & 0xFF] | detail::MortonLUT<uint64_t>::y[y & 0xFF] | detail::MortonLUT<uint64_t>::z[z & 0xFF];
		return result;
	}

	inline uint32_t MortonDecode64(uint64_t c)
	{
		c &= detail::MortonMAX<uint64_t,sizeof(uint64_t)>::value;
		c = (c ^ (c >> 2))  & 0x30C30C30C30C30C3ULL;
		c = (c ^ (c >> 4))  & 0xF00F00F00F00F00FULL;
		c = (c ^ (c >> 8))  & 0x00FF0000FF0000FFULL;
		c = (c ^ (c >> 16)) & 0xFFFF00000000FFFFULL;
		c = (c ^ (c >> 32)) & 0x00000000FFFFFFFFULL;
		return static_cast<uint32_t>(c);
	}

	inline uint32_t MortonDecode64_X(uint64_t c)
	{
		return MortonDecode64(c);
	}

	inline uint32_t MortonDecode64_Y(uint64_t c)
	{
		return MortonDecode64(c >> 1);
	}

	inline uint32_t MortonDecode64_Z(uint64_t c)
	{
		return MortonDecode64(c >> 2);
	}

	inline uint32_t MortonEncode32(uint16_t x, uint16_t y, uint16_t z)
	{
		uint32_t result = detail::MortonLUT<uint32_t>::x[(x >> 8) & 0x07] | detail::MortonLUT<uint32_t>::y[(y >> 8) & 0x07] | detail::MortonLUT<uint32_t>::z[(z >> 8) & 0x03];
		result <<= 24;
		result |= detail::MortonLUT<uint32_t>::x[x & 0xFF] | detail::MortonLUT<uint32_t>::y[y & 0xFF] | detail::MortonLUT<uint32_t>::z[z & 0xFF];
		return result;
	}

	inline uint16_t MortonDecode32(uint32_t c)
	{
		c &= detail::MortonMAX<uint32_t,sizeof(uint32_t)>::value;
		c = (c ^ (c >> 2))  & 0xC30C30C3;
		c = (c ^ (c >> 4))  & 0x0F00F00F;
		c = (c ^ (c >> 8))  & 0xFF0000FF;
		c = (c ^ (c >> 16)) & 0x0000FFFF;
		return static_cast<uint16_t>(c);
	}

	inline uint16_t MortonDecode32_X(uint32_t c)
	{
		return MortonDecode32(c);
	}

	inline uint16_t MortonDecode32_Y(uint32_t c)
	{
		return MortonDecode32(c >> 1);
	}

	inline uint16_t MortonDecode32_Z(uint32_t c)
	{
		return MortonDecode32(c >> 2);
	}

	inline uint16_t MortonEncode16(uint8_t x, uint8_t y, uint8_t z)
	{
		return detail::MortonLUT<uint16_t>::x[x & 0x3F] | detail::MortonLUT<uint16_t>::y[y & 0x1F] | detail::MortonLUT<uint16_t>::z[z & 0x1F];
	}

	inline uint8_t MortonDecode16(uint16_t c)
	{
		c &= detail::MortonMAX<uint16_t,sizeof(uint16_t)>::value;
		c = (c ^ (c >> 2)) & 0x30C3;
		c = (c ^ (c >> 4)) & 0xF00F;
		c = (c ^ (c >> 8)) & 0x00FF;
		return static_cast<uint8_t>(c);
	}

	inline uint8_t MortonDecode16_X(uint16_t c)
	{
		return MortonDecode16(c);
	}

	inline uint8_t MortonDecode16_Y(uint16_t c)
	{
		return MortonDecode16(c >> 1);
	}

	inline uint8_t MortonDecode16_Z(uint16_t c)
	{
		return MortonDecode16(c >> 2);
	}
}

#endif /* OOBASE_INCLUDE_OOBASE_MORTON_H_ */
