#include <common/DocumentElements.h>
#include <odr/Document.h>
#include <odr/DocumentElements.h>

namespace odr::common {

ElementType TextElement::type() const { return ElementType::TEXT; }

ElementType Paragraph::type() const { return ElementType::PARAGRAPH; }

ElementType Span::type() const { return ElementType::SPAN; }

ElementType Link::type() const { return ElementType::LINK; }

ElementType Bookmark::type() const { return ElementType::BOOKMARK; }

ElementType List::type() const { return ElementType::LIST; }

ElementType ListItem::type() const { return ElementType::LIST_ITEM; }

ElementType Table::type() const { return ElementType::TABLE; }

ElementType TableColumn::type() const { return ElementType::TABLE_COLUMN; }

ElementType TableRow::type() const { return ElementType::TABLE_ROW; }

ElementType TableCell::type() const { return ElementType::TABLE_CELL; }

ElementType Frame::type() const { return ElementType::FRAME; }

ElementType Image::type() const { return ElementType::IMAGE; }

ElementType Rect::type() const { return ElementType::RECT; }

ElementType Line::type() const { return ElementType::LINE; }

ElementType Circle::type() const { return ElementType::CIRCLE; }

} // namespace odr::common
