/*
 *
 * Copyright 2021-2025 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 *
 * srsRAN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsRAN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

 #pragma once

 #include "srsran/ran/logical_channel/lcid.h"
 #include "fmt/format.h"
 
 namespace srsran {
 
 constexpr std::size_t MAX_NOF_SRBS = 4;
 
 enum class srb_id_t : uint8_t { srb0 = 0, srb1, srb2, srb3, nulltype };
 
 constexpr inline std::underlying_type_t<srb_id_t> srb_id_to_uint(srb_id_t id)
 {
   return static_cast<std::underlying_type_t<srb_id_t>>(id);
 }
 
 inline srb_id_t int_to_srb_id(std::underlying_type_t<srb_id_t> val)
 {
   return static_cast<srb_id_t>(val);
 }
 
 inline srb_id_t to_srb_id(lcid_t lcid)
 {
   return is_srb(lcid) ? static_cast<srb_id_t>(lcid) : srb_id_t::nulltype;
 }
 
 /// Convert SRB ID to LCID.
 inline lcid_t srb_id_to_lcid(srb_id_t srb_id)
 {
   return static_cast<lcid_t>(srb_id);
 }
 
 inline const char* srb_id_to_string(srb_id_t srb_id)
 {
   static constexpr const char* names[] = {"SRB0", "SRB1", "SRB2", "SRB3", "invalid"};
   return names[srb_id_to_uint(srb_id < srb_id_t::nulltype ? srb_id : srb_id_t::nulltype)];
 }
 
 enum class drb_id_t : uint8_t {
   drb1 = 1,
   drb2,
   drb3,
   drb4,
   drb5,
   drb6,
   drb7,
   drb8,
   drb9,
   drb10,
   drb11,
   drb12,
   drb13,
   drb14,
   drb15,
   drb16,
   drb17,
   drb18,
   drb19,
   drb20,
   drb21,
   drb22,
   drb23,
   drb24,
   drb25,
   drb26,
   drb27,
   drb28,
   drb29,
   invalid
 };
 
 static constexpr std::size_t MAX_NOF_DRBS = 29;
 
 constexpr inline uint8_t drb_id_to_uint(drb_id_t id)
 {
   return static_cast<uint8_t>(id);
 }
 
 constexpr inline drb_id_t uint_to_drb_id(uint8_t id)
 {
   return static_cast<drb_id_t>(id);
 }
 
 /// Radio Bearer type, either SRB or DRB.
 enum class rb_type_t { srb, drb };
 
 /// Radio Bearer Identity, e.g. SRB1, DRB1, DRB2,...
 class rb_id_t
 {
 public:
   rb_id_t() = default;
   rb_id_t(srb_id_t srb_id_) : rb_type(rb_type_t::srb), srb_id(srb_id_) {}
   rb_id_t(drb_id_t drb_id_) : rb_type(rb_type_t::drb), drb_id(drb_id_){};
 
   bool is_srb() const { return rb_type == rb_type_t::srb; }
   bool is_drb() const { return rb_type == rb_type_t::drb; }
 
   srb_id_t get_srb_id() const { return srb_id; }
   drb_id_t get_drb_id() const { return drb_id; }
 
 private:
   rb_type_t rb_type;
   union {
     srb_id_t srb_id;
     drb_id_t drb_id;
   };
 };
 
 } // namespace srsran
 
 namespace fmt {
 
 // drb_id_t formatter
 template <>
 struct formatter<srsran::drb_id_t> {
   template <typename ParseContext>
   auto parse(ParseContext& ctx)
   {
     return ctx.begin();
   }
 
   template <typename FormatContext>
   auto format(srsran::drb_id_t o, FormatContext& ctx) const
   {
     switch (o) {
       case srsran::drb_id_t::invalid:
         return format_to(ctx.out(), "invalid DRB");
       default:
         return format_to(ctx.out(), "DRB{}", drb_id_to_uint(o));
     }
   }
 };
 
 // srb_id_t formatter
 template <>
 struct formatter<srsran::srb_id_t> {
   template <typename ParseContext>
   auto parse(ParseContext& ctx)
   {
     return ctx.begin();
   }
 
   template <typename FormatContext>
   auto format(srsran::srb_id_t o, FormatContext& ctx) const
   {
     return format_to(ctx.out(), "SRB{}", srb_id_to_uint(o));
   }
 };
 
 // rb_id_t formatter
 template <>
 struct formatter<srsran::rb_id_t> {
   template <typename ParseContext>
   auto parse(ParseContext& ctx)
   {
     return ctx.begin();
   }
 
   template <typename FormatContext>
   auto format(srsran::rb_id_t o, FormatContext& ctx) const
   {
     if (o.is_drb()) {
       return format_to(ctx.out(), "{}", o.get_drb_id());
     }
     if (o.is_srb()) {
       return format_to(ctx.out(), "{}", o.get_srb_id());
     }
     return format_to(ctx.out(), "Invalid");
   }
 };
 
 } // namespace fmt
 